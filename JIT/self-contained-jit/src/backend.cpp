#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/Shared/ExecutorSymbolDef.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/TargetSelect.h"
#include <cstdint>
#include <iostream>
#include <sstream>

llvm::ExitOnError exit_on_error;

class Type {
public:
  virtual ~Type() = default;
  virtual llvm::Type *get(llvm::LLVMContext &) const = 0;
};

class IntegerType : public Type {
public:
  IntegerType() {}
  llvm::Type *get(llvm::LLVMContext &context) const {
    return llvm::Type::getInt32Ty(context);
  }
};

extern "C" Type *create_integer_type() { return new IntegerType; }

class PointerType : public Type {
public:
  PointerType() {}
  llvm::Type *get(llvm::LLVMContext &context) const {
    return llvm::PointerType::get(context, 0);
  }
};

extern "C" Type *create_pointer_type() { return new PointerType; }

class Expression {
public:
  virtual ~Expression() = default;
  virtual llvm::Value *codegen(llvm::IRBuilderBase &) const = 0;
  virtual void debug_print(std::ostream &) const = 0;
  virtual Expression *to_constructor() const = 0;
};

struct Call : Expression {
  const char *function_name;
  Type *return_type;
  std::vector<Type *> parameters_type;
  std::vector<Expression *> arguments;

public:
  Call(const char *function_name, Type *return_type,
       const std::vector<Type *> &parameters_type,
       const std::vector<Expression *> &arguments)
      : function_name(function_name), return_type(return_type),
        parameters_type(parameters_type), arguments(arguments) {}
  llvm::Value *codegen(llvm::IRBuilderBase &builder) const override {
    llvm::Type *llvm_return_type = return_type->get(builder.getContext());
    std::vector<llvm::Type *> llvm_parameters_type;
    for (auto &parameter_type : parameters_type) {
      llvm_parameters_type.push_back(parameter_type->get(builder.getContext()));
    }
    llvm::FunctionType *function_type =
        llvm::FunctionType::get(llvm_return_type, llvm_parameters_type, false);
    auto module = builder.GetInsertBlock()->getModule();
    llvm::Function *function = module->getFunction(function_name);
    if (!function) {
      function =
          llvm::Function::Create(function_type, llvm::Function::ExternalLinkage,
                                 function_name, module);
    }
    std::vector<llvm::Value *> arguments_value;
    for (auto &argument : arguments) {
      arguments_value.push_back(argument->codegen(builder));
    }
    return builder.CreateCall(function_type, function, arguments_value);
  }
  void debug_print(std::ostream &os) const override {
    os << "Call " << function_name << "(";
    for (std::size_t argument_index = 0; argument_index < arguments.size();
         argument_index++) {
      arguments[argument_index]->debug_print(os);
      if (argument_index < arguments.size() - 1) {
        os << ", ";
      }
    }
    os << ")";
  }
  Expression *to_constructor() const override;
};

extern "C" Call *create_call(const char *function_name, Type *return_type,
                             Type **parameters_type, std::size_t num_parameters,
                             Expression **arguments) {
  std::vector<Type *> vec_parameters_type;
  std::vector<Expression *> vec_arguments;
  for (std::size_t parameter_index = 0; parameter_index < num_parameters;
       parameter_index++) {
    vec_parameters_type.push_back(parameters_type[parameter_index]);
    vec_arguments.push_back(arguments[parameter_index]);
  }
  return new Call(function_name, return_type, vec_parameters_type,
                  vec_arguments);
}

struct Integer : Expression {
  std::int32_t value;

public:
  Integer(std::int32_t value) : value(value) {}
  llvm::Value *codegen(llvm::IRBuilderBase &builder) const override {
    return builder.getInt32(value);
  }
  void debug_print(std::ostream &os) const override {
    os << "Integer " << value;
  }
  Expression *to_constructor() const override {
    Type *return_type = new PointerType;
    std::vector<Type *> parameters_type = {new IntegerType};
    std::vector<Expression *> arguments = {new Integer(value)};
    return new Call("create_integer", return_type, parameters_type, arguments);
  }
};

extern "C" Integer *create_integer(std::int32_t value) {
  return new Integer(value);
}

struct Pointer : Expression {
  std::size_t value;

public:
  Pointer(std::size_t value) : value(value) {}
  llvm::Value *codegen(llvm::IRBuilderBase &builder) const override {
    llvm::BasicBlock *basic_block = builder.GetInsertBlock();
    const llvm::DataLayout &data_layout = basic_block->getDataLayout();
    return llvm::ConstantInt::get(builder.getIntPtrTy(data_layout), value);
  }
  void debug_print(std::ostream &os) const override {
    os << "Pointer " << value;
  }
  Expression *to_constructor() const override {
    Type *return_type = new PointerType;
    std::vector<Type *> parameters_type = {new PointerType};
    std::vector<Expression *> arguments = {new Pointer(value)};
    return new Call("create_pointer", return_type, parameters_type, arguments);
  }
};

extern "C" Pointer *create_pointer(std::size_t value) {
  return new Pointer(value);
}

struct List : Expression {
  Type *type;
  std::vector<Expression *> elements;

public:
  List(Type *type, std::vector<Expression *> elements)
      : type(type), elements(elements) {}
  llvm::Value *codegen(llvm::IRBuilderBase &builder) const override {
    llvm::Type *element_type = type->get(builder.getContext());
    llvm::ArrayType *array_type =
        llvm::ArrayType::get(element_type, elements.size());
    llvm::Value *array = builder.CreateAlloca(array_type);
    for (std::size_t element_index = 0; element_index < elements.size();
         element_index++) {
      llvm::Value *element = elements[element_index]->codegen(builder);
      llvm::Value *pointer =
          builder.CreateConstGEP2_64(array_type, array, 0, element_index);
      builder.CreateStore(element, pointer);
    }
    return array;
  }
  void debug_print(std::ostream &os) const override {
    os << "List(";
    for (std::size_t element_index = 0; element_index < elements.size();
         element_index++) {
      elements[element_index]->debug_print(os);
      if (element_index < elements.size() - 1) {
        os << ", ";
      }
    }
    os << ")";
  }
  Expression *to_constructor() const override {
    std::vector<Type *> new_parameters_type = {
        new PointerType,
        new PointerType,
        new PointerType,
    };
    std::vector<Expression *> elements_constructor;
    for (Expression *element : elements) {
      elements_constructor.push_back(element->to_constructor());
    }
    std::vector<Expression *> new_arguments = {
        new Pointer(reinterpret_cast<std::size_t>(type)),
        new List(new PointerType, elements_constructor),
        new Pointer(elements.size()),
    };
    return new Call("create_list", new PointerType, new_parameters_type,
                    new_arguments);
  }
};

extern "C" List *create_list(Type *type, Expression **elements,
                             std::size_t num_elements) {
  std::vector<Expression *> vec_elements;
  for (std::size_t element_index = 0; element_index < num_elements;
       element_index++) {
    vec_elements.push_back(elements[element_index]);
  }
  return new List(type, vec_elements);
}

extern "C" Expression *to_constructor(Expression *expression) {
  return expression->to_constructor();
}

extern "C" void debug_print(Expression *expression) {
  expression->debug_print(std::cout);
  std::cout << std::endl;
}

std::unique_ptr<llvm::orc::LLJIT> jit;

extern "C" void initialize_jit() {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::orc::LLJITBuilder jit_builder;
  jit = exit_on_error(jit_builder.create());
  char global_prefix = jit->getDataLayout().getGlobalPrefix();
  auto generator = exit_on_error(
      llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
          global_prefix));
  jit->getMainJITDylib().addGenerator(std::move(generator));
}

extern "C" void *compile_expression(Expression *expression, Type *return_type,
                                    Type **parameters_type,
                                    std::size_t num_parameters) {
  std::stringstream function_name_builder;
  function_name_builder << expression;
  std::string function_name = function_name_builder.str();
  auto context = std::make_unique<llvm::LLVMContext>();
  llvm::Type *llvm_return_type = return_type->get(*context);
  std::vector<llvm::Type *> llvm_parameters_type;
  for (std::size_t parameter_index = 0; parameter_index < num_parameters;
       parameter_index++) {
    llvm_parameters_type.push_back(
        parameters_type[parameter_index]->get(*context));
  }
  llvm::FunctionType *function_type =
      llvm::FunctionType::get(llvm_return_type, llvm_parameters_type, false);
  auto module = std::make_unique<llvm::Module>("", *context);
  llvm::Function *function = llvm::Function::Create(
      function_type, llvm::Function::ExternalLinkage, function_name, *module);
  llvm::IRBuilder builder(*context);
  llvm::BasicBlock *basic_block =
      llvm::BasicBlock::Create(*context, "entry", function);
  builder.SetInsertPoint(basic_block);
  llvm::Value *ret = expression->codegen(builder);
  builder.CreateRet(ret);
  // module->print(llvm::outs(), nullptr);
  exit_on_error(jit->addIRModule(
      llvm::orc::ThreadSafeModule(std::move(module), std::move(context))));
  auto symbol = exit_on_error(jit->lookup(function_name));
  return symbol.toPtr<void *>();
}

Expression *Call::to_constructor() const {
  std::vector<Type *> new_parameters_type = {
      new PointerType, new PointerType, new PointerType,
      new PointerType, new PointerType,
  };
  std::vector<Expression *> parameters_type_constructor;
  for (Type *parameter_type : parameters_type) {
    parameters_type_constructor.push_back(
        new Pointer(reinterpret_cast<std::size_t>(parameter_type)));
  }
  std::vector<Expression *> arguments_constructor;
  for (Expression *argument : arguments) {
    arguments_constructor.push_back(argument->to_constructor());
  }
  std::vector<Expression *> new_arguments = {
      new Pointer(reinterpret_cast<std::size_t>(function_name)),
      new Pointer(reinterpret_cast<std::size_t>(return_type)),
      new List(new PointerType, parameters_type_constructor),
      new Pointer(parameters_type.size()),
      new List(new PointerType, arguments_constructor),
  };
  return new Call("create_call", new PointerType, new_parameters_type,
                  new_arguments);
}
