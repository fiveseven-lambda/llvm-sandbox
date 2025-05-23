#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Error.h"

class Type {
public:
  virtual ~Type();
  virtual llvm::Type *into_llvm_type(llvm::LLVMContext &) const = 0;
};

class BooleanType : public Type {
public:
  llvm::Type *into_llvm_type(llvm::LLVMContext &) const override;
};

extern "C" BooleanType *get_boolean_type();

class IntegerType : public Type {
public:
  llvm::Type *into_llvm_type(llvm::LLVMContext &) const override;
};

extern "C" IntegerType *get_integer_type();

class SizeType : public Type {
public:
  llvm::Type *into_llvm_type(llvm::LLVMContext &) const override;
};

extern "C" SizeType *get_size_type();

class StringType : public Type {
public:
  llvm::Type *into_llvm_type(llvm::LLVMContext &) const override;
};

extern "C" StringType *get_string_type();

struct TypeContext {
  BooleanType boolean_type;
  IntegerType integer_type;
  SizeType size_type;
  StringType string_type;
};

class Expression {
public:
  void *pointer;
  Expression();
  virtual ~Expression();
  virtual llvm::Value *codegen(llvm::IRBuilderBase &) const = 0;
  virtual void debug_print(std::ostream &) const = 0;
  virtual Expression *to_constructor() const = 0;
};

extern "C" void debug_print(Expression *);

extern "C" Expression *to_constructor(Expression *);

class Parameter : public Expression {
  int index;

public:
  Parameter(int);
  llvm::Value *codegen(llvm::IRBuilderBase &) const override;
  void debug_print(std::ostream &) const override;
  Expression *to_constructor() const override;
};

extern "C" Parameter *create_parameter(int);

class Boolean : public Expression {
  bool value;

public:
  Boolean(bool);
  llvm::Value *codegen(llvm::IRBuilderBase &) const override;
  void debug_print(std::ostream &) const override;
  Expression *to_constructor() const override;
};

extern "C" Boolean *create_boolean(bool);

class Integer : public Expression {
  int value;

public:
  Integer(int);
  llvm::Value *codegen(llvm::IRBuilderBase &) const override;
  void debug_print(std::ostream &) const override;
  Expression *to_constructor() const override;
};

extern "C" Integer *create_integer(int);

class AddInteger : public Expression {
  Expression *left, *right;

public:
  AddInteger(Expression *, Expression *);
  llvm::Value *codegen(llvm::IRBuilderBase &) const override;
  void debug_print(std::ostream &) const override;
  Expression *to_constructor() const override;
};

extern "C" AddInteger *create_add_integer(Expression *, Expression *);

class Size : public Expression {
  std::size_t value;

public:
  Size(std::size_t);
  llvm::Value *codegen(llvm::IRBuilderBase &) const override;
  void debug_print(std::ostream &) const override;
  Expression *to_constructor() const override;
};

extern "C" Size *create_size(std::size_t);

class String : public Expression {
  std::size_t length;
  const char *pointer;

public:
  String(std::size_t, const char *);
  llvm::Value *codegen(llvm::IRBuilderBase &) const override;
  void debug_print(std::ostream &) const override;
  Expression *to_constructor() const override;
};

extern "C" String *create_string(std::size_t, const char *);

class Print : public Expression {
  Expression *string;

public:
  Print(Expression *);
  llvm::Value *codegen(llvm::IRBuilderBase &) const override;
  void debug_print(std::ostream &) const override;
  Expression *to_constructor() const override;
};

extern "C" Print *create_print(Expression *);

class Array : public Expression {
  Type *type;
  std::vector<Expression *> elements;

public:
  Array(Type *, std::vector<Expression *>);
  llvm::Value *codegen(llvm::IRBuilderBase &) const override;
  void debug_print(std::ostream &) const override;
  Expression *to_constructor() const override;
};

extern "C" Array *create_array(Type *, std::size_t, Expression **);

class Function : public Expression {
  const char *name;
  Type *return_type;
  std::vector<Type *> parameters_type;
  bool is_variadic;

public:
  Function(const char *, Type *, std::vector<Type *>, bool);
  llvm::Value *codegen(llvm::IRBuilderBase &) const override;
  void debug_print(std::ostream &) const override;
  Expression *to_constructor() const override;
};

extern "C" Function *create_function(const char *, Type *, std::size_t, Type **,
                                     bool);

class Call : public Expression {
  Expression *function;
  Type *return_type;
  std::vector<Type *> parameters_type;
  bool is_variadic;
  std::vector<Expression *> arguments;

public:
  Call(Expression *, Type *, const std::vector<Type *> &, bool,
       const std::vector<Expression *> &);
  llvm::Value *codegen(llvm::IRBuilderBase &) const override;
  void debug_print(std::ostream &) const override;
  Expression *to_constructor() const override;
};

extern "C" Call *create_call(Expression *, Type *, std::size_t, Type **, bool,
                             Expression **);

extern "C" void initialize_jit();

extern "C" void *compile_expression(Expression *, Type *, std::size_t, Type **);

struct Context {
  std::unique_ptr<llvm::LLVMContext> llvm_context;
  llvm::IRBuilder<llvm::ConstantFolder, llvm::IRBuilderDefaultInserter> builder;
  std::unique_ptr<llvm::Module> module;
  std::vector<llvm::BasicBlock *> basic_blocks;

public:
  Context();
};

extern "C" Context *create_context();

extern "C" llvm::Function *add_function(Context *, const char *name, Type *,
                                        std::size_t, Type **, std::size_t);

extern "C" void set_insert_point(Context *, std::size_t);

extern "C" void add_expression(Context *, Expression *);

extern "C" void add_return(Context *, Expression *);

extern "C" void *compile(Context *, const char *);

extern "C" void delete_context(Context *);
