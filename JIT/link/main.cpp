#include <iostream>
#include <gnu/lib-names.h>

#include "llvm/IR/IRBuilder.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"

int main(){
	// executor process control の生成
	llvm::Expected<std::unique_ptr<llvm::orc::SelfExecutorProcessControl>> error_or_executor_process_control = llvm::orc::SelfExecutorProcessControl::Create();
	if(!error_or_executor_process_control){
		llvm::errs() << error_or_executor_process_control.takeError() << '\n';
		return -1;
	}
	std::unique_ptr<llvm::orc::SelfExecutorProcessControl> &executor_process_control = error_or_executor_process_control.get();

	// execusion session の生成
	llvm::orc::ExecutionSession execution_session(std::move(executor_process_control));

	// target machine
	LLVMInitializeNativeTarget();
	std::unique_ptr<llvm::TargetMachine> target_machine(llvm::EngineBuilder().selectTarget());

	llvm::DataLayout data_layout = target_machine->createDataLayout();

	llvm::orc::ObjectLinkingLayer object_linking_layer(execution_session);

	LLVMInitializeNativeAsmPrinter();
	auto compiler = std::make_unique<llvm::orc::SimpleCompiler>(*target_machine);
	llvm::orc::IRCompileLayer compile_layer(execution_session, object_linking_layer, std::move(compiler));

	// execusion session 上に JIT dynamic library を生成
	llvm::Expected<llvm::orc::JITDylib &> error_or_main_dynamic_library = execution_session.createJITDylib("main");
	if(!error_or_main_dynamic_library){
		llvm::errs() << error_or_main_dynamic_library.takeError() << '\n';
		return -1;
	}
	llvm::orc::JITDylib &main_dynamic_library = error_or_main_dynamic_library.get();

	if (auto d = llvm::orc::DynamicLibrarySearchGenerator::Load(LIBC_SO, data_layout.getGlobalPrefix())) {
		main_dynamic_library.addGenerator(std::move(*d));
	}
	/*
	if (auto d = llvm::orc::DynamicLibrarySearchGenerator::Load(LIBM_SO, data_layout.getGlobalPrefix())) {
		main_dynamic_library.addGenerator(std::move(*d));
	}
	*/
	
	auto lookup_and_getAddress = [&](const char fnc_name[]) -> unsigned long {
		// JIT dynamic library から関数を探す
		llvm::Expected<llvm::JITEvaluatedSymbol> error_or_jit_evaluated_symbol = execution_session.lookup({&main_dynamic_library}, fnc_name);
		if(!error_or_jit_evaluated_symbol){
			std::cerr << "failed to evaluate" << fnc_name << std::endl;
			llvm::errs() << error_or_jit_evaluated_symbol.takeError() << '\n';
			return -1;
		}
		llvm::JITEvaluatedSymbol &jit_evaluated_symbol = error_or_jit_evaluated_symbol.get();

		// アドレスを手に入れて関数へとキャスト
		return jit_evaluated_symbol.getAddress();
	};

	llvm::orc::ThreadSafeContext context(std::make_unique<llvm::LLVMContext>());
	llvm::IRBuilder<llvm::ConstantFolder, llvm::IRBuilderDefaultInserter> builder(*context.getContext());

	llvm::Type *double_type = llvm::Type::getDoubleTy(*context.getContext());
	llvm::FunctionType *function_type = llvm::FunctionType::get(double_type, {double_type}, false);
	llvm::PointerType *function_pointer_type = llvm::PointerType::get(function_type, 0);
	auto main_module = std::make_unique<llvm::Module>("main", *context.getContext());
	llvm::Function *function = llvm::Function::Create(function_type, llvm::Function::ExternalLinkage, "fnc", *main_module);

	// これでもいい
	llvm::Type *int64_type = llvm::Type::getInt64Ty(*context.getContext());
	llvm::Value* sin_function = llvm::ConstantExpr::getIntToPtr(
		llvm::ConstantInt::get(int64_type, (int64_t)&sin),
		function_pointer_type
	);
	// libm.so から探す場合
	// llvm::Function *sin_function = llvm::Function::Create(function_type, llvm::Function::ExternalLinkage, "sin", *main_module);

	llvm::BasicBlock *basic_block = llvm::BasicBlock::Create(*context.getContext(), "entry", function);
	builder.SetInsertPoint(basic_block);
	llvm::Value *x = function->getArg(0);
	llvm::Value *sin_x = builder.CreateCall(function_type, sin_function, {x});
	builder.CreateRet(sin_x);

	main_module->print(llvm::errs(), nullptr);
	llvm::errs() << '\n';

	if(auto error = compile_layer.add(main_dynamic_library, llvm::orc::ThreadSafeModule(std::move(main_module), context))){
		llvm::errs() << error << '\n';
	}

	auto printf_addr = reinterpret_cast<int(*)(...)>(lookup_and_getAddress("printf"));
	auto sin_addr = reinterpret_cast<double(*)(double)>(lookup_and_getAddress("sin"));
	auto fnc_addr = reinterpret_cast<double(*)(double)>(lookup_and_getAddress("fnc"));

	main_dynamic_library.dump(llvm::errs());
	printf_addr("sin(1) = %f\n", fnc_addr(1));
}
