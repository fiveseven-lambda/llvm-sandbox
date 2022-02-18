#include <iostream>
#include <memory>
#include <utility>

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Error.h"

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"

#include "llvm/Target/TargetMachine.h"

#include "llvm/IR/DataLayout.h"

#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutorProcessControl.h"
#include "llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/ObjectTransformLayer.h"

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

	// resource tracker
	llvm::IntrusiveRefCntPtr<llvm::orc::ResourceTracker> resource_tracker = main_dynamic_library.getDefaultResourceTracker();

	// object linking layer
	llvm::orc::ObjectLinkingLayer object_linking_layer(execution_session);

	// object transform layer
	llvm::orc::ObjectTransformLayer object_transform_layer(execution_session, object_linking_layer);

	// printf が定義されたファイル
	char file_name[] = "/lib/libc.so.6";

	// libc の中身をバッファに読む
	llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> error_or_buffer = llvm::MemoryBuffer::getFile(file_name);
	if(!error_or_buffer){
		std::cerr << "failed to open " << file_name << ": " << error_or_buffer.getError().message() << std::endl;
		return -1;
	}
	std::unique_ptr<llvm::MemoryBuffer> &buffer = error_or_buffer.get();

	// libc の中身を object transform layer
	if(llvm::Error error = object_transform_layer.add(resource_tracker, std::move(buffer))){
		std::cerr << "failed to add " << file_name << " to JIT" << std::endl;
		llvm::errs() << error << '\n';
		return -1;
	}

	main_dynamic_library.dump(llvm::errs());
	
	char fnc_name[] = "printf";

	// JIT dynamic library から printf を探す
	llvm::Expected<llvm::JITEvaluatedSymbol> error_or_jit_evaluated_symbol = execution_session.lookup({&main_dynamic_library}, fnc_name);
	if(!error_or_jit_evaluated_symbol){
		std::cerr << "failed to evaluate " << fnc_name << std::endl;
		llvm::errs() << error_or_jit_evaluated_symbol.takeError() << '\n';
		return -1;
	}
	llvm::JITEvaluatedSymbol &jit_evaluated_symbol = error_or_jit_evaluated_symbol.get();

	// アドレスを手に入れて関数へとキャスト
	auto printf_addr = reinterpret_cast<int(*)(...)>(jit_evaluated_symbol.getAddress());

	// 呼び出し
	std::cout << printf_addr("%d", 10) << std::endl;
}
