#include <iostream>
#include <memory>
#include <utility>

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Error.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"

#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutorProcessControl.h"
#include "llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/ObjectTransformLayer.h"
#include "llvm/ExecutionEngine/JITSymbol.h"

int main(){
	// executor process control って何だよ
	llvm::Expected<std::unique_ptr<llvm::orc::SelfExecutorProcessControl>> error_or_executor_process_control = llvm::orc::SelfExecutorProcessControl::Create();
	if(!error_or_executor_process_control){
		llvm::errs() << error_or_executor_process_control.takeError() << '\n';
		return -1;
	}
	std::unique_ptr<llvm::orc::SelfExecutorProcessControl> &executor_process_control = error_or_executor_process_control.get();

	// execusion session って何だよ
	llvm::orc::ExecutionSession execution_session(std::move(executor_process_control));

	// JIT dynamic library って何だよ
	llvm::Expected<llvm::orc::JITDylib &> error_or_main_dynamic_library = execution_session.createJITDylib("main");
	if(!error_or_main_dynamic_library){
		llvm::errs() << error_or_main_dynamic_library.takeError() << '\n';
		return -1;
	}
	llvm::orc::JITDylib &main_dynamic_library = error_or_main_dynamic_library.get();

	// resource tracker って何だよ
	llvm::IntrusiveRefCntPtr<llvm::orc::ResourceTracker> resource_tracker = main_dynamic_library.getDefaultResourceTracker();

	// object linking layer って何だよ
	llvm::orc::ObjectLinkingLayer object_linking_layer(execution_session);

	// object transform layer って何だよ
	llvm::orc::ObjectTransformLayer object_transform_layer(execution_session, object_linking_layer);

	// fnc_in_c が定義されたオブジェクトファイル（事前に生成済み）
	char filename[] = "tmp.o";

	// バッファに読む
	llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> error_or_buffer = llvm::MemoryBuffer::getFile(filename);
	if(!error_or_buffer){
		std::cerr << "failed to open " << filename << ": " << error_or_buffer.getError().message() << std::endl;
		return -1;
	}
	std::unique_ptr<llvm::MemoryBuffer> &buffer = error_or_buffer.get();

	// object transform layer（何だよ）に追加
	if(llvm::Error error = object_transform_layer.add(resource_tracker, std::move(buffer))){
		std::cerr << "failed to add " << filename << " to JIT" << std::endl;
		llvm::errs() << error << '\n';
		return -1;
	}

	// ダンプすると fnc_in_c が見つかる
	main_dynamic_library.dump(llvm::errs());

	// JIT dynamic library（何だよ）から fnc_in_c を探す
	llvm::Expected<llvm::JITEvaluatedSymbol> error_or_jit_evaluated_symbol = execution_session.lookup({&main_dynamic_library}, "fnc_in_c");
	if(!error_or_jit_evaluated_symbol){
		llvm::errs() << error_or_jit_evaluated_symbol.takeError() << '\n';
		return -1;
	}
	llvm::JITEvaluatedSymbol &jit_evaluated_symbol = error_or_jit_evaluated_symbol.get();

	// アドレスを手に入れて関数へとキャスト
	int (*function)(int) = reinterpret_cast<int(*)(int)>(jit_evaluated_symbol.getAddress());

	// 呼び出し
	std::cout << function(10) << std::endl;
}
