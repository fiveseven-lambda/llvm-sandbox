#include <iostream>

#include "llvm/IR/IRBuilder.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"

int main(){
    llvm::orc::ThreadSafeContext context(std::make_unique<llvm::LLVMContext>());

    // i32
    llvm::Type *integer_type = llvm::Type::getInt32Ty(*context.getContext());
    // void
    llvm::Type *void_type = llvm::Type::getVoidTy(*context.getContext());
    // add_one 関数の型 i32 (i32)
    llvm::FunctionType *add_one_type = llvm::FunctionType::get(integer_type, {integer_type}, false);
    // pointer の型 i32 (i32)*
    llvm::PointerType *pointer_type = llvm::PointerType::getUnqual(add_one_type);
    // store 関数の型 void ()
    llvm::FunctionType *store_type = llvm::FunctionType::get(void_type, {}, false);

    // モジュール
    auto main_module = std::make_unique<llvm::Module>("main", *context.getContext());
    /*
     * ここから，モジュールの中身をこのようにしたい：
    
        @pointer = global i32 (i32)* null
        
        define void @store() {
            store i32 (i32)* @add_one, i32 (i32)** @pointer
            ret void
        }
        
        define private i32 @add_one(i32 %0) {
            %2 = add i32 %0, 1
            ret i32 %2
        }

     */

    // store 関数
    llvm::Function *store_function = llvm::Function::Create(store_type, llvm::Function::ExternalLinkage, "store", *main_module);
    // 1 個目の builder
    llvm::IRBuilder builder1(*context.getContext());
    // 関数定義開始
    llvm::BasicBlock *store_entry = llvm::BasicBlock::Create(*context.getContext(), "", store_function);
    builder1.SetInsertPoint(store_entry);
    llvm::Function *add_one_function;
    {
        // add_one 関数
        add_one_function = llvm::Function::Create(add_one_type, llvm::Function::PrivateLinkage, "add_one", *main_module);
        // 2 個目の builder
        llvm::IRBuilder builder2(*context.getContext());
        // 関数定義開始
        llvm::BasicBlock *add_one_entry = llvm::BasicBlock::Create(*context.getContext(), "", add_one_function);
        builder2.SetInsertPoint(add_one_entry);
        llvm::Value *x = add_one_function->getArg(0);
        builder2.CreateRet(builder2.CreateAdd(x, builder2.getInt32(1)));
        // 関数定義終了
    }
    llvm::Value *pointer = new llvm::GlobalVariable(*main_module, pointer_type, false, llvm::Function::ExternalLinkage, llvm::ConstantPointerNull::get(pointer_type), "pointer");
    builder1.CreateStore(add_one_function, pointer);
    builder1.CreateRetVoid();
    // 関数定義終了

    // LLVM IR の出力
    main_module->print(llvm::errs(), nullptr);

    // 以下，実行
    llvm::Expected<std::unique_ptr<llvm::orc::SelfExecutorProcessControl>> error_or_executor_process_control = llvm::orc::SelfExecutorProcessControl::Create();
    if(!error_or_executor_process_control){
        llvm::errs() << error_or_executor_process_control.takeError() << '\n';
        return -1;
    }
    std::unique_ptr<llvm::orc::SelfExecutorProcessControl> &executor_process_control = error_or_executor_process_control.get();
    llvm::orc::ExecutionSession execution_session(std::move(executor_process_control));
    LLVMInitializeNativeTarget();
    std::unique_ptr<llvm::TargetMachine> target_machine(llvm::EngineBuilder().selectTarget());
    llvm::DataLayout data_layout = target_machine->createDataLayout();
    llvm::orc::ObjectLinkingLayer object_linking_layer(execution_session);
    LLVMInitializeNativeAsmPrinter();
    auto compiler = std::make_unique<llvm::orc::SimpleCompiler>(*target_machine);
    llvm::orc::IRCompileLayer compile_layer(execution_session, object_linking_layer, std::move(compiler));
    llvm::Expected<llvm::orc::JITDylib &> error_or_main_dynamic_library = execution_session.createJITDylib("main");
    if(!error_or_main_dynamic_library){
        llvm::errs() << error_or_main_dynamic_library.takeError() << '\n';
        return -1;
    }
    llvm::orc::JITDylib &main_dynamic_library = error_or_main_dynamic_library.get();
    if(auto error = compile_layer.add(main_dynamic_library, llvm::orc::ThreadSafeModule(std::move(main_module), context))){
        llvm::errs() << error << '\n';
    }
    auto lookup_and_getAddress = [&](const char fnc_name[]) -> unsigned long {
        llvm::Expected<llvm::JITEvaluatedSymbol> error_or_jit_evaluated_symbol = execution_session.lookup({&main_dynamic_library}, fnc_name);
        if(!error_or_jit_evaluated_symbol){
            std::cerr << "failed to evaluate" << fnc_name << std::endl;
            llvm::errs() << error_or_jit_evaluated_symbol.takeError() << '\n';
            return -1;
        }
        llvm::JITEvaluatedSymbol &jit_evaluated_symbol = error_or_jit_evaluated_symbol.get();
        return jit_evaluated_symbol.getAddress();
    };
    auto store_addr = reinterpret_cast<void(*)()>(lookup_and_getAddress("store"));
    auto add_one_addr = reinterpret_cast<int(**)(int)>(lookup_and_getAddress("pointer"));
    store_addr();
    std::cout << (*add_one_addr)(10) << std::endl;
}
