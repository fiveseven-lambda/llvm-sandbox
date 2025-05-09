#include "backend.hpp"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/TargetSelect.h"
#include <cstdint>
#include <iostream>
#include <sstream>

static TypeContext global_type_context;

static llvm::ExitOnError exit_on_error;

static std::unique_ptr<llvm::orc::LLJIT> jit;

Type::~Type() = default;

llvm::Type *BooleanType::into_llvm_type(llvm::LLVMContext &context) const {
  return llvm::Type::getInt1Ty(context);
}

extern "C" BooleanType *get_boolean_type() {
  return &global_type_context.boolean_type;
}

llvm::Type *IntegerType::into_llvm_type(llvm::LLVMContext &context) const {
  return llvm::Type::getInt32Ty(context);
}

extern "C" IntegerType *get_integer_type() {
  return &global_type_context.integer_type;
}

llvm::Type *SizeType::into_llvm_type(llvm::LLVMContext &context) const {
  return llvm::IntegerType::get(context, sizeof(std::size_t) * CHAR_BIT);
}

extern "C" SizeType *get_size_type() { return &global_type_context.size_type; }

llvm::Type *StringType::into_llvm_type(llvm::LLVMContext &context) const {
  llvm::Type *field_type = get_size_type()->into_llvm_type(context);
  return llvm::StructType::get(field_type, field_type);
}

extern "C" StringType *get_string_type() {
  return &global_type_context.string_type;
}

Expression::~Expression() = default;

extern "C" void debug_print(Expression *expression) {
  expression->debug_print(std::cout);
  std::cout << std::endl;
}

extern "C" Expression *to_constructor(Expression *expression) {
  return expression->to_constructor();
}

Boolean::Boolean(bool value) : value(value) {}

llvm::Value *Boolean::codegen(llvm::IRBuilderBase &builder) const {
  return builder.getInt1(value);
}

void Boolean::debug_print(std::ostream &os) const { os << "Boolean " << value; }

Expression *Boolean::to_constructor() const {
  Type *return_type = get_size_type();
  std::vector<Type *> parameters_type = {get_boolean_type()};
  std::vector<Expression *> arguments = {new Boolean(value)};
  return new Call("create_boolean", return_type, parameters_type, arguments,
                  false);
}

extern "C" Boolean *create_boolean(bool value) { return new Boolean(value); }

Integer::Integer(std::int32_t value) : value(value) {}

llvm::Value *Integer::codegen(llvm::IRBuilderBase &builder) const {
  return builder.getInt32(value);
}

void Integer::debug_print(std::ostream &os) const { os << "Integer " << value; }

Expression *Integer::to_constructor() const {
  Type *return_type = get_size_type();
  std::vector<Type *> parameters_type = {get_integer_type()};
  std::vector<Expression *> arguments = {new Integer(value)};
  return new Call("create_integer", return_type, parameters_type, arguments,
                  false);
}

extern "C" Integer *create_integer(std::int32_t value) {
  return new Integer(value);
}

Size::Size(std::size_t value) : value(value) {}

llvm::Value *Size::codegen(llvm::IRBuilderBase &builder) const {
  llvm::Type *type = get_size_type()->into_llvm_type(builder.getContext());
  return llvm::ConstantInt::get(type, value);
}

void Size::debug_print(std::ostream &os) const { os << "Size " << value; }

Expression *Size::to_constructor() const {
  Type *return_type = get_size_type();
  std::vector<Type *> parameters_type = {get_size_type()};
  std::vector<Expression *> arguments = {new Size(value)};
  return new Call("create_size", return_type, parameters_type, arguments,
                  false);
}

extern "C" Size *create_size(std::size_t value) { return new Size(value); }

String::String(std::size_t length, const char *pointer)
    : length(length), pointer(pointer) {}

llvm::Value *String::codegen(llvm::IRBuilderBase &builder) const {
  llvm::Type *type = get_size_type()->into_llvm_type(builder.getContext());
  return llvm::ConstantStruct::get(
      llvm::StructType::get(type, type), llvm::ConstantInt::get(type, length),
      llvm::ConstantInt::get(type, reinterpret_cast<std::size_t>(pointer)));
}

void String::debug_print(std::ostream &os) const {
  os << "String \"" << std::string_view(pointer, length) << "\"";
}

Expression *String::to_constructor() const {
  Type *return_type = get_size_type();
  std::vector<Type *> parameters_type = {get_size_type(), get_size_type()};
  std::vector<Expression *> arguments = {
      new Size(length), new Size(reinterpret_cast<std::size_t>(pointer))};
  return new Call("create_string", return_type, parameters_type, arguments,
                  false);
}

extern "C" String *create_string(std::size_t length, const char *pointer) {
  return new String(length, pointer);
}

Print::Print(Expression *string) : string(string) {}

llvm::Value *Print::codegen(llvm::IRBuilderBase &builder) const {
  llvm::Type *llvm_integer_type =
      get_integer_type()->into_llvm_type(builder.getContext());
  llvm::Type *llvm_size_type =
      get_size_type()->into_llvm_type(builder.getContext());
  llvm::Value *llvm_string = string->codegen(builder);
  llvm::Value *length = builder.CreateExtractValue(llvm_string, {0});
  llvm::Value *pointer = builder.CreateExtractValue(llvm_string, {1});
  std::vector<Type *> parameters_type = {get_size_type()};

  llvm::Value *format = llvm::ConstantInt::get(
      llvm_size_type, reinterpret_cast<std::size_t>("%.*s"));

  llvm::FunctionType *function_type =
      llvm::FunctionType::get(llvm_integer_type, {llvm_size_type}, true);
  auto module = builder.GetInsertBlock()->getModule();
  llvm::Function *function = module->getFunction("printf");
  if (!function) {
    function = llvm::Function::Create(
        function_type, llvm::Function::ExternalLinkage, "printf", module);
  }
  std::vector<llvm::Value *> arguments_value = {
      format,
      length,
      pointer,
  };
  return builder.CreateCall(function_type, function, arguments_value);
}

void Print::debug_print(std::ostream &os) const {
  os << "Print(";
  string->debug_print(os);
  os << ")";
}

Expression *Print::to_constructor() const {
  std::vector<Type *> parameters_type = {get_size_type()};
  std::vector<Expression *> arguments = {string->to_constructor()};
  return new Call("create_print", get_size_type(), parameters_type, arguments,
                  false);
}

extern "C" Print *create_print(Expression *string) { return new Print(string); }

Array::Array(Type *type, std::vector<Expression *> elements)
    : type(type), elements(elements) {}

llvm::Value *Array::codegen(llvm::IRBuilderBase &builder) const {
  llvm::Type *element_type = type->into_llvm_type(builder.getContext());
  llvm::ArrayType *array_type =
      llvm::ArrayType::get(element_type, elements.size());
  llvm::Value *array = builder.CreateAlloca(array_type);
  for (std::size_t element_index = 0; element_index < elements.size();
       element_index++) {
    llvm::Value *element = elements[element_index]->codegen(builder);
    llvm::Value *size =
        builder.CreateConstGEP2_64(array_type, array, 0, element_index);
    builder.CreateStore(element, size);
  }
  return array;
}

void Array::debug_print(std::ostream &os) const {
  os << "Array(";
  for (std::size_t element_index = 0; element_index < elements.size();
       element_index++) {
    elements[element_index]->debug_print(os);
    if (element_index < elements.size() - 1) {
      os << ", ";
    }
  }
  os << ")";
}

Expression *Array::to_constructor() const {
  std::vector<Type *> new_parameters_type(3, get_size_type());
  std::vector<Expression *> elements_constructor;
  for (Expression *element : elements) {
    elements_constructor.push_back(element->to_constructor());
  }
  std::vector<Expression *> new_arguments = {
      new Size(reinterpret_cast<std::size_t>(type)),
      new Size(elements.size()),
      new Array(get_size_type(), elements_constructor),
  };
  return new Call("create_array", get_size_type(), new_parameters_type,
                  new_arguments, false);
}

extern "C" Array *create_array(Type *type, std::size_t num_elements,
                               Expression **elements) {
  std::vector<Expression *> vec_elements;
  for (std::size_t element_index = 0; element_index < num_elements;
       element_index++) {
    vec_elements.push_back(elements[element_index]);
  }
  return new Array(type, vec_elements);
}

Call::Call(const char *function_name, Type *return_type,
           const std::vector<Type *> &parameters_type,
           const std::vector<Expression *> &arguments, bool is_variadic)
    : function_name(function_name), return_type(return_type),
      parameters_type(parameters_type), arguments(arguments),
      is_variadic(is_variadic) {}

llvm::Value *Call::codegen(llvm::IRBuilderBase &builder) const {
  llvm::Type *llvm_return_type =
      return_type->into_llvm_type(builder.getContext());
  std::vector<llvm::Type *> llvm_parameters_type;
  for (auto &parameter_type : parameters_type) {
    llvm_parameters_type.push_back(
        parameter_type->into_llvm_type(builder.getContext()));
  }
  llvm::FunctionType *function_type = llvm::FunctionType::get(
      llvm_return_type, llvm_parameters_type, is_variadic);
  auto module = builder.GetInsertBlock()->getModule();
  llvm::Function *function = module->getFunction(function_name);
  if (!function) {
    function = llvm::Function::Create(
        function_type, llvm::Function::ExternalLinkage, function_name, module);
  }
  std::vector<llvm::Value *> arguments_value;
  for (auto &argument : arguments) {
    arguments_value.push_back(argument->codegen(builder));
  }
  return builder.CreateCall(function_type, function, arguments_value);
}

void Call::debug_print(std::ostream &os) const {
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

Expression *Call::to_constructor() const {
  std::vector<Type *> new_parameters_type(5, get_size_type());
  new_parameters_type.push_back(get_boolean_type());
  std::vector<Expression *> parameters_type_constructor;
  for (Type *parameter_type : parameters_type) {
    parameters_type_constructor.push_back(
        new Size(reinterpret_cast<std::size_t>(parameter_type)));
  }
  std::vector<Expression *> arguments_constructor;
  for (Expression *argument : arguments) {
    arguments_constructor.push_back(argument->to_constructor());
  }
  std::vector<Expression *> new_arguments = {
      new Size(reinterpret_cast<std::size_t>(function_name)),
      new Size(reinterpret_cast<std::size_t>(return_type)),
      new Size(parameters_type.size()),
      new Array(get_size_type(), parameters_type_constructor),
      new Array(get_size_type(), arguments_constructor),
      new Boolean(is_variadic)};
  return new Call("create_call", get_size_type(), new_parameters_type,
                  new_arguments, false);
}

extern "C" Call *create_call(const char *function_name, Type *return_type,
                             std::size_t num_parameters, Type **parameters_type,
                             Expression **arguments, bool is_variadic) {
  std::vector<Type *> vec_parameters_type;
  std::vector<Expression *> vec_arguments;
  for (std::size_t parameter_index = 0; parameter_index < num_parameters;
       parameter_index++) {
    vec_parameters_type.push_back(parameters_type[parameter_index]);
    vec_arguments.push_back(arguments[parameter_index]);
  }
  return new Call(function_name, return_type, vec_parameters_type,
                  vec_arguments, is_variadic);
}

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
                                    std::size_t num_parameters,
                                    Type **parameters_type) {
  std::stringstream function_name_builder;
  function_name_builder << expression;
  std::string function_name = function_name_builder.str();
  auto context = std::make_unique<llvm::LLVMContext>();
  llvm::Type *llvm_return_type = return_type->into_llvm_type(*context);
  std::vector<llvm::Type *> llvm_parameters_type;
  for (std::size_t parameter_index = 0; parameter_index < num_parameters;
       parameter_index++) {
    llvm_parameters_type.push_back(
        parameters_type[parameter_index]->into_llvm_type(*context));
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
