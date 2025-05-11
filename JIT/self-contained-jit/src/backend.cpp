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
  return llvm::IntegerType::get(context, sizeof(int) * CHAR_BIT);
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

Expression::Expression() : pointer(nullptr) {}

Expression::~Expression() = default;

extern "C" void debug_print(Expression *expression) {
  expression->debug_print(std::cout);
  std::cout << std::endl;
}

extern "C" Expression *to_constructor(Expression *expression) {
  return expression->to_constructor();
}

Parameter::Parameter(int index) : index(index) {}

llvm::Value *Parameter::codegen(llvm::IRBuilderBase &builder) const {
  return builder.GetInsertBlock()->getParent()->getArg(index);
}

void Parameter::debug_print(std::ostream &os) const {
  os << "Parameter " << index;
}

Expression *Parameter::to_constructor() const {
  return new Call(new Function("create_parameter", get_size_type(),
                               {get_integer_type()}, false),
                  get_size_type(), {get_integer_type()}, false,
                  {new Integer(index)});
}

extern "C" Parameter *create_parameter(int index) {
  return new Parameter(index);
}

Boolean::Boolean(bool value) : value(value) {}

llvm::Value *Boolean::codegen(llvm::IRBuilderBase &builder) const {
  return builder.getInt1(value);
}

void Boolean::debug_print(std::ostream &os) const { os << "Boolean " << value; }

Expression *Boolean::to_constructor() const {
  return new Call(new Function("create_boolean", get_size_type(),
                               {get_boolean_type()}, false),
                  get_size_type(), {get_boolean_type()}, false,
                  {new Boolean(value)});
}

extern "C" Boolean *create_boolean(bool value) { return new Boolean(value); }

Integer::Integer(int value) : value(value) {}

llvm::Value *Integer::codegen(llvm::IRBuilderBase &builder) const {
  llvm::Type *integer_type =
      get_integer_type()->into_llvm_type(builder.getContext());
  return llvm::ConstantInt::get(integer_type, value);
}

void Integer::debug_print(std::ostream &os) const { os << "Integer " << value; }

Expression *Integer::to_constructor() const {
  return new Call(new Function("create_integer", get_size_type(),
                               {get_integer_type()}, false),
                  get_size_type(), {get_integer_type()}, false,
                  {new Integer(value)});
}

extern "C" Integer *create_integer(int value) { return new Integer(value); }

AddInteger::AddInteger(Expression *left, Expression *right)
    : left(left), right(right) {}

llvm::Value *AddInteger::codegen(llvm::IRBuilderBase &builder) const {
  llvm::Value *llvm_left = left->codegen(builder);
  llvm::Value *llvm_right = right->codegen(builder);
  return builder.CreateAdd(llvm_left, llvm_right);
}

void AddInteger::debug_print(std::ostream &os) const {
  os << "AddInteger(";
  left->debug_print(os);
  os << ", ";
  right->debug_print(os);
  os << ")";
}

Expression *AddInteger::to_constructor() const {
  Expression *left_constructor = left->to_constructor();
  Expression *right_constructor = right->to_constructor();
  return new Call(new Function("create_add_integer", get_size_type(),
                               {get_size_type(), get_size_type()}, false),
                  get_size_type(), {get_size_type(), get_size_type()}, false,
                  {left_constructor, right_constructor});
}

extern "C" AddInteger *create_add_integer(Expression *left, Expression *right) {
  return new AddInteger(left, right);
}

Size::Size(std::size_t value) : value(value) {}

llvm::Value *Size::codegen(llvm::IRBuilderBase &builder) const {
  llvm::Type *type = get_size_type()->into_llvm_type(builder.getContext());
  return llvm::ConstantInt::get(type, value);
}

void Size::debug_print(std::ostream &os) const { os << "Size " << value; }

Expression *Size::to_constructor() const {
  return new Call(
      new Function("create_size", get_size_type(), {get_size_type()}, false),
      get_size_type(), {get_size_type()}, false, {new Size(value)});
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
  return new Call(
      new Function("create_string", get_size_type(),
                   {get_size_type(), get_size_type()}, false),
      get_size_type(), {get_size_type(), get_size_type()}, false,
      {new Size(length), new Size(reinterpret_cast<std::size_t>(pointer))});
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
  return builder.CreateCall(function_type, function, {format, length, pointer});
}

void Print::debug_print(std::ostream &os) const {
  os << "Print(";
  string->debug_print(os);
  os << ")";
}

Expression *Print::to_constructor() const {
  return new Call(
      new Function("create_print", get_size_type(), {get_size_type()}, false),
      get_size_type(), {get_size_type()}, false, {string->to_constructor()});
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
  std::vector<Expression *> elements_constructor;
  for (Expression *element : elements) {
    elements_constructor.push_back(element->to_constructor());
  }
  return new Call(
      new Function("create_array", get_size_type(),
                   {get_size_type(), get_size_type(), get_size_type()}, false),
      get_size_type(), {get_size_type(), get_size_type(), get_size_type()},
      false,
      {
          new Size(reinterpret_cast<std::size_t>(type)),
          new Size(elements.size()),
          new Array(get_size_type(), elements_constructor),
      });
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

Function::Function(const char *name, Type *return_type,
                   std::vector<Type *> parameters_type, bool is_variadic)
    : name(name), return_type(return_type), parameters_type(parameters_type),
      is_variadic(is_variadic) {}

llvm::Value *Function::codegen(llvm::IRBuilderBase &builder) const {
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
  llvm::Function *function = module->getFunction(name);
  if (!function) {
    function = llvm::Function::Create(
        function_type, llvm::Function::ExternalLinkage, name, module);
  }
  llvm::Type *llvm_size_type =
      get_size_type()->into_llvm_type(builder.getContext());
  llvm::FunctionType *type_create_ready_made =
      llvm::FunctionType::get(llvm_size_type, {llvm_size_type}, false);
  llvm::Function *llvm_create_ready_made =
      module->getFunction("create_ready_made");
  if (!llvm_create_ready_made) {
    llvm_create_ready_made =
        llvm::Function::Create(function_type, llvm::Function::ExternalLinkage,
                               "create_ready_made", module);
  }
  return builder.CreateCall(type_create_ready_made, llvm_create_ready_made,
                            {function});
}

extern "C" Expression *create_ready_made(void *pointer) {
  Expression *ret =
      reinterpret_cast<Expression *>(operator new(sizeof(Expression)));
  ret->pointer = pointer;
  return ret;
}

void Function::debug_print(std::ostream &os) const {
  os << "Function " << name;
}

Expression *Function::to_constructor() const {
  std::vector<Expression *> parameters_type_constructor;
  for (Type *parameter_type : parameters_type) {
    parameters_type_constructor.push_back(
        new Size(reinterpret_cast<std::size_t>(parameter_type)));
  }
  return new Call(
      new Function("create_function", get_size_type(),
                   {get_size_type(), get_size_type(), get_size_type(),
                    get_size_type(), get_boolean_type()},
                   false),
      get_size_type(),
      {get_size_type(), get_size_type(), get_size_type(), get_size_type(),
       get_boolean_type()},
      false,
      {
          new Size(reinterpret_cast<std::size_t>(name)),
          new Size(reinterpret_cast<std::size_t>(return_type)),
          new Size(parameters_type.size()),
          new Array(get_size_type(), parameters_type_constructor),
          new Boolean(is_variadic),
      });
}

Function *create_function(const char *name, Type *return_type,
                          std::size_t num_parameters, Type **parameters_type,
                          bool is_variadic) {
  std::vector<Type *> vec_parameters_type;
  for (std::size_t parameter_index = 0; parameter_index < num_parameters;
       parameter_index++) {
    vec_parameters_type.push_back(parameters_type[parameter_index]);
  }
  return new Function(name, return_type, vec_parameters_type, is_variadic);
}

Call::Call(Expression *function, Type *return_type,
           const std::vector<Type *> &parameters_type, bool is_variadic,
           const std::vector<Expression *> &arguments)
    : function(function), return_type(return_type),
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

  llvm::Value *llvm_function = function->codegen(builder);

  llvm::Type *llvm_size_type =
      get_size_type()->into_llvm_type(builder.getContext());
  llvm::FunctionType *compile_expression_type = llvm::FunctionType::get(
      llvm_size_type,
      {llvm_size_type, llvm_size_type, llvm_size_type, llvm_size_type}, false);
  auto module = builder.GetInsertBlock()->getModule();
  llvm::Function *llvm_compile_expression =
      module->getFunction("compile_expression");
  if (!llvm_compile_expression) {
    llvm_compile_expression = llvm::Function::Create(
        compile_expression_type, llvm::Function::ExternalLinkage,
        "compile_expression", module);
  }
  llvm::Value *llvm_function_pointer = builder.CreateCall(
      compile_expression_type, llvm_compile_expression,
      {
          llvm_function,
          llvm::ConstantInt::get(llvm_size_type,
                                 reinterpret_cast<std::size_t>(return_type)),
          llvm::ConstantInt::get(llvm_size_type, parameters_type.size()),
          llvm::ConstantInt::get(llvm_size_type, reinterpret_cast<std::size_t>(
                                                     &parameters_type[0])),
      });

  std::vector<llvm::Value *> arguments_value;
  for (auto &argument : arguments) {
    arguments_value.push_back(argument->codegen(builder));
  }
  return builder.CreateCall(function_type, llvm_function_pointer,
                            arguments_value);
}

void Call::debug_print(std::ostream &os) const {
  os << "Call ";
  function->debug_print(os);
  os << "(";
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
  std::vector<Expression *> parameters_type_constructor;
  for (Type *parameter_type : parameters_type) {
    parameters_type_constructor.push_back(
        new Size(reinterpret_cast<std::size_t>(parameter_type)));
  }
  std::vector<Expression *> arguments_constructor;
  for (Expression *argument : arguments) {
    arguments_constructor.push_back(argument->to_constructor());
  }
  return new Call(new Function("create_call", get_size_type(),
                               {
                                   get_size_type(),
                                   get_size_type(),
                                   get_size_type(),
                                   get_size_type(),
                                   get_boolean_type(),
                                   get_size_type(),
                               },
                               false),
                  get_size_type(),
                  {
                      get_size_type(),
                      get_size_type(),
                      get_size_type(),
                      get_size_type(),
                      get_boolean_type(),
                      get_size_type(),
                  },
                  false,
                  {new Size(reinterpret_cast<std::size_t>(function)),
                   new Size(reinterpret_cast<std::size_t>(return_type)),
                   new Size(parameters_type.size()),
                   new Array(get_size_type(), parameters_type_constructor),
                   new Boolean(is_variadic),
                   new Array(get_size_type(), arguments_constructor)});
}

extern "C" Call *create_call(Expression *function, Type *return_type,
                             std::size_t num_parameters, Type **parameters_type,
                             bool is_variadic, Expression **arguments) {
  std::vector<Type *> vec_parameters_type;
  std::vector<Expression *> vec_arguments;
  for (std::size_t parameter_index = 0; parameter_index < num_parameters;
       parameter_index++) {
    vec_parameters_type.push_back(parameters_type[parameter_index]);
    vec_arguments.push_back(arguments[parameter_index]);
  }
  return new Call(function, return_type, vec_parameters_type, is_variadic,
                  vec_arguments);
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
  if (!expression->pointer) {
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
        llvm::BasicBlock::Create(*context, "", function);
    builder.SetInsertPoint(basic_block);
    llvm::Value *ret = expression->codegen(builder);
    builder.CreateRet(ret);
    // module->print(llvm::outs(), nullptr);
    exit_on_error(jit->addIRModule(
        llvm::orc::ThreadSafeModule(std::move(module), std::move(context))));
    auto symbol = exit_on_error(jit->lookup(function_name));
    expression->pointer = symbol.toPtr<void *>();
  }
  return expression->pointer;
}
