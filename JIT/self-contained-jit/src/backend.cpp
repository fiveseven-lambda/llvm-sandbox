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
  virtual Expression *to_constructor(llvm::LLVMContext &) const = 0;
};

struct Call : Expression {
  std::string function_name;
  Type *return_type;
  std::vector<Type *> parameters_type;
  std::vector<Expression *> arguments;

public:
  Call(std::string function_name, Type *return_type,
       std::vector<Type *> parameters_type, std::vector<Expression *> arguments)
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
    for (auto &argument : arguments) {
      argument->debug_print(os);
    }
    os << ")";
  }
  Expression *to_constructor(llvm::LLVMContext &context) const override {
    return nullptr;
  }
};

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
  Expression *to_constructor(llvm::LLVMContext &context) const override {
    std::string function_name("create_integer");
    Type *return_type = new IntegerType;
    std::vector<Type *> parameters_type = {new PointerType};
    std::vector<Expression *> arguments = {new Integer(value)};
    return new Call(function_name, return_type, parameters_type, arguments);
  }
};

extern "C" Integer *create_integer(std::int32_t value) {
  return new Integer(value);
}

extern "C" Expression *to_constructor(Expression *expression) {
  llvm::LLVMContext context;
  return expression->to_constructor(context);
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
  exit_on_error(jit->addIRModule(
      llvm::orc::ThreadSafeModule(std::move(module), std::move(context))));
  auto symbol = exit_on_error(jit->lookup(function_name));
  return symbol.toPtr<void *>();
}
