#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <syscall.h>
#include <vector>

#include <cstdio>
#include <cassert>
#include <unistd.h>
#include <sys/types.h>
#include <strings.h>
// stat is not supported in Pin 3.0
// #include <sys/stat.h>
#include <sys/syscall.h>
#include <string.h>
#include <pthread.h>

#include "pin.H"

#include "globals.h"
#include "threads.h"
#include "recorder_control.h"
#include "recorder_base.h"
#include "syscall_modeling.h"
#include "trace_rtn.h"
#include "emulation.h"
#include "sift_writer.h"
#include "sift_assert.h"
#include "pinboost_debug.h"

#include "/home/yxz7776/sniper/common/misc/subsecond_time.h"

// #include "/home/yxz7776/sniper/common/core/core.h"
// #include "/home/yxz7776/sniper/pin/local_storage.h"

#define MODIFIED

#if defined(MODIFIED)
// static int inst_count = 0;
#endif

VOID Fini(INT32 code, VOID* v)
{
    for (unsigned int i = 0; i < max_num_threads; i++) {
        if (thread_data[i].output) {
            closeFile(i);
        }
    }
}

VOID Detach(VOID* v)
{
}

BOOL followChild(CHILD_PROCESS childProcess, VOID* val)
{
    if (any_thread_in_detail) {
        fprintf(stderr, "EXECV ignored while in ROI\n");
        return false; // Cannot fork/execv after starting ROI
    } else
        return true;
}

VOID forkBefore(THREADID threadid, const CONTEXT* ctxt, VOID* v)
{
    if (thread_data[threadid].output) {
        child_app_id = thread_data[threadid].output->Fork();
    }
}

VOID forkAfterInChild(THREADID threadid, const CONTEXT* ctxt, VOID* v)
{
    // Forget about everything we inherited from the parent
    routines.clear();
    bzero(thread_data, max_num_threads * sizeof(*thread_data));
    // Assume identity of child process
    app_id = child_app_id;
    num_threads = 1;
    // Open new SIFT pipe for thread 0
    thread_data[0].bbv = new Bbv();
    openFile(0);
}

bool assert_ignore()
{
    // stat is not supported in Pin 3.0
    // this code just check if the file exists or not
    // struct stat st;
    // if (stat((KnobOutputFile.Value() + ".sift_done").c_str(), &st) == 0)
    //    return true;
    // else
    //    return false;
    if (FILE* file = fopen((KnobOutputFile.Value() + ".sift_done").c_str(), "rb")) {
        fclose(file);
        return true;
    } else {
        return false;
    }
}

void __sift_assert_fail(__const char* __assertion, __const char* __file,
    unsigned int __line, __const char* __function)
    __THROW
{
    if (assert_ignore()) {
        // Timing model says it's done, ignore assert and pretend to have exited cleanly
        exit(0);
    } else {
        std::cerr << "[SIFT_RECORDER] " << __file << ":" << __line << ": " << __function
                  << ": Assertion `" << __assertion << "' failed." << std::endl;
        abort();
    }
}

#if defined(MODIFIED)
VOID AnalyzeContext(ADDRINT ip, CONTEXT* ctxt)
{
    //  PIN_REGISTER reg_val;
    //  PIN_GetContextRegval(ctxt, LEVEL_BASE::REG_RAX, reinterpret_cast<UINT8 *>(&reg_val));
    // //  std::cout << std::hex << "REG_RAX: 0x" << LEVEL_VM::PIN_REGISTER::reg_val << std::endl;
    // std::cout << std::hex << "REG_GAX: 0x" << PIN_GetContextReg(ctxt, LEVEL_BASE::REG_GAX) << std::endl;

    Sift::EmuRequest req;
    Sift::EmuReply res;
    bool emulated = thread_data[0].output->Emulate(Sift::EmuTypeRdtsc, req, res);
    if (emulated) {
        std::cerr << "Current # of executed cycle:" << res.rdtsc.cycles << std::endl;
    }

    // Core * core = localStore[0].thread->getCore();
    // assert (core);
    // SubsecondTime cycles_fs = core->getPerformanceModel()->getElapsedTime();
}

static VOID InstrumentInstructionByThreshold(
    INS ins,
    VOID* v)
{
    RTN Parent = INS_Rtn(ins);
    if (false
        || !(RTN_Valid(Parent))
        || (RTN_Name(Parent) != "main"))
        return;

    // inst_count++;
    // if (10 != inst_count)
    //     return;
    ComponentTime::ComponentTime identifier = new ComponentTime();
    if (ComponentTime::identifier.get_interrupt_ready() == 0)
        return;

    // PerformanceModel* prfmdl = getPerformanceModel();
    // SubsecondTime cycles_fs = prfmdl->getElapsedTime();

    INS_InsertPredicatedCall(ins,
        IPOINT_BEFORE, // decide if you want to point before or IPOINT_AFTER
        (AFUNPTR)AnalyzeContext, // analysis routine
        IARG_INST_PTR, // address of the ins
        IARG_CONST_CONTEXT, // the const context (DO NOT MODIFY it in the analysis!)
        IARG_END);

    std::cerr << "sift_recorder->InstrumentInstructionByThreshold: Ian walked by..." << std::endl;
    std::cerr << INS_Address(ins) << " : " << INS_Disassemble(ins) << std::endl;
}
#endif

int main(int argc, char **argv)
{
   if (PIN_Init(argc,argv))
   {
      std::cerr << "Error, invalid parameters" << std::endl;
      std::cerr << KNOB_BASE::StringKnobSummary() << std::endl;
      exit(1);
   }
   PIN_InitSymbols();

   if (KnobMaxThreads.Value() > 0)
   {
      max_num_threads = KnobMaxThreads.Value();
   }
   size_t thread_data_size = max_num_threads * sizeof(*thread_data);
   if (posix_memalign((void**)&thread_data, LINE_SIZE_BYTES, thread_data_size) != 0)
   {
      std::cerr << "Error, posix_memalign() failed" << std::endl;
      exit(1);
   }
   bzero(thread_data, thread_data_size);

   PIN_InitLock(&access_memory_lock);
   PIN_InitLock(&new_threadid_lock);

   app_id = KnobSiftAppId.Value();
   blocksize = KnobBlocksize.Value();
   fast_forward_target = KnobFastForwardTarget.Value();
   detailed_target = KnobDetailedTarget.Value();
   if (KnobEmulateSyscalls.Value() || (!KnobUseROI.Value() && !KnobMPIImplicitROI.Value()))
   {
      if (app_id < 0)
         findMyAppId();
   }
   if (fast_forward_target == 0 && !KnobUseROI.Value() && !KnobMPIImplicitROI.Value())
   {
      in_roi = true;
      setInstrumentationMode(Sift::ModeDetailed);
      openFile(0);
   }
   else if (KnobEmulateSyscalls.Value())
   {
      openFile(0);
   }

   // When attaching with --pid, there could be a number of threads already running.
   // Manually call NewThread() because the normal method to start new thread pipes (SYS_clone)
   // will already have happened
   if (PIN_GetInitialThreadCount() > 1)
   {
      sift_assert(thread_data[PIN_ThreadId()].output);
      for (UINT32 i = 1 ; i < PIN_GetInitialThreadCount() ; i++)
      {
         thread_data[PIN_ThreadId()].output->NewThread();
      }
   }

#ifdef PINPLAY_SUPPORTED
   if (KnobReplayer.Value())
   {
      if (KnobEmulateSyscalls.Value())
      {
         std::cerr << "Error, emulating syscalls is not compatible with PinPlay replaying." << std::endl;
         exit(1);
      }
      pinplay_engine.Activate(argc, argv, false /*logger*/, KnobReplayer.Value() /*replayer*/);
   }
#else
   if (KnobReplayer.Value())
   {
      std::cerr << "Error, PinPlay support not compiled in. Please use a compatible pin kit when compiling." << std::endl;
      exit(1);
   }
#endif

   if (KnobEmulateSyscalls.Value())
   {
      if (!KnobUseResponseFiles.Value())
      {
         std::cerr << "Error, Response files are required when using syscall emulation." << std::endl;
         exit(1);
      }

      initSyscallModeling();
   }

   initThreads();
   initRecorderControl();
   initRecorderBase();
   initEmulation();

   if (KnobRoutineTracing.Value())
      initRoutineTracer();

   PIN_AddFiniFunction(Fini, 0);
   PIN_AddDetachFunction(Detach, 0);

   PIN_AddFollowChildProcessFunction(followChild, 0);
   if (KnobEmulateSyscalls.Value())
   {
      PIN_AddForkFunction(FPOINT_BEFORE, forkBefore, 0);
      PIN_AddForkFunction(FPOINT_AFTER_IN_CHILD, forkAfterInChild, 0);
   }

   pinboost_register("SIFT_RECORDER", KnobDebug.Value());

#if defined(MODIFIED)
   std::cerr << "In sift(pin) side, Ian is visiting..." << std::endl;

   // PIN_REGISTER * gax;
   // PIN_REGISTER * gdx;
   // handleRdtsc(0, REG_GAX, REG_GDX);
   // std::cerr << thread_data

   INS_AddInstrumentFunction(
       InstrumentInstructionByThreshold,
       nullptr);
#endif

   PIN_StartProgram();

   return 0;
}
