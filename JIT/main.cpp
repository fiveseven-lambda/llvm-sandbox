#include <iostream>

#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"

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

	// data layout
	llvm::DataLayout data_layout = target_machine->createDataLayout();

	// execusion session 上に JIT dynamic library を生成
	llvm::Expected<llvm::orc::JITDylib &> error_or_main_dynamic_library = execution_session.createJITDylib("main");
	if(!error_or_main_dynamic_library){
		llvm::errs() << error_or_main_dynamic_library.takeError() << '\n';
		return -1;
	}
	llvm::orc::JITDylib &main_dynamic_library = error_or_main_dynamic_library.get();

	if (auto d = llvm::orc::DynamicLibrarySearchGenerator::Load("/usr/lib/libc.so.6", data_layout.getGlobalPrefix())) {
		main_dynamic_library.addGenerator(std::move(*d));
	}
	if (auto d = llvm::orc::DynamicLibrarySearchGenerator::Load("/usr/lib/libm.so.6", data_layout.getGlobalPrefix())) {
		main_dynamic_library.addGenerator(std::move(*d));
	}
	
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

	auto printf_addr = reinterpret_cast<int(*)(...)>(lookup_and_getAddress("printf"));
	auto sin_addr = reinterpret_cast<double(*)(double)>(lookup_and_getAddress("sin"));

	// 呼び出し
	printf_addr("%f\n", sin_addr(1));
}
