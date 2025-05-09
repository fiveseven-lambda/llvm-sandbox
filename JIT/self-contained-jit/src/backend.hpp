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

class IntegerType : public Type {
public:
  llvm::Type *into_llvm_type(llvm::LLVMContext &) const;
};

extern "C" IntegerType *get_integer_type();

class PointerType : public Type {
public:
  llvm::Type *into_llvm_type(llvm::LLVMContext &) const;
};

extern "C" PointerType *get_pointer_type();

struct TypeContext {
  IntegerType integer_type;
  PointerType pointer_type;
};

class Expression {
public:
  virtual ~Expression() = default;
  virtual llvm::Value *codegen(llvm::IRBuilderBase &) const = 0;
  virtual void debug_print(std::ostream &) const = 0;
  virtual Expression *to_constructor() const = 0;
};

extern "C" void debug_print(Expression *);

extern "C" Expression *to_constructor(Expression *);

struct Integer : Expression {
  std::int32_t value;

public:
  Integer(std::int32_t);
  llvm::Value *codegen(llvm::IRBuilderBase &) const override;
  void debug_print(std::ostream &) const override;
  Expression *to_constructor() const override;
};

extern "C" Integer *create_integer(std::int32_t);

struct Pointer : Expression {
  std::size_t value;

public:
  Pointer(std::size_t);
  llvm::Value *codegen(llvm::IRBuilderBase &) const override;
  void debug_print(std::ostream &) const override;
  Expression *to_constructor() const override;
};

extern "C" Pointer *create_pointer(std::size_t);

struct List : Expression {
  Type *type;
  std::vector<Expression *> elements;

public:
  List(Type *, std::vector<Expression *>);
  llvm::Value *codegen(llvm::IRBuilderBase &) const override;
  void debug_print(std::ostream &) const override;
  Expression *to_constructor() const override;
};

extern "C" List *create_list(Type *, Expression **, std::size_t);

struct Call : Expression {
  const char *function_name;
  Type *return_type;
  std::vector<Type *> parameters_type;
  std::vector<Expression *> arguments;

public:
  Call(const char *, Type *, const std::vector<Type *> &,
       const std::vector<Expression *> &);
  llvm::Value *codegen(llvm::IRBuilderBase &) const override;
  void debug_print(std::ostream &) const override;
  Expression *to_constructor() const override;
};

extern "C" Call *create_call(const char *, Type *, Type **, std::size_t,
                             Expression **);

extern "C" void initialize_jit();

extern "C" void *compile_expression(Expression *, Type *, Type **, std::size_t);
