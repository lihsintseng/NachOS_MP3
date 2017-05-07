// scheduler.cc 
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would 
//	end up calling FindNextToRun(), and that would put us in an 
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

// function edited
Scheduler::Scheduler()
{ 
//
    readyList = new SortedList<Thread *>(Thread::compare_by_priority); //priority
    readyList_RR = new List<Thread *>();	//RR(CPU initially done by RR)
    readyList_SJF = new SortedList<Thread *>(Thread::compare_by_burst); //SJF is burst time related
//
    toBeDestroyed = NULL;
} 

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------
//function edited
Scheduler::~Scheduler()
{ 
    //
    delete readyList_RR;
    delete readyList_SJF;
    //
    delete readyList; 
} 

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------
//function edited
void
Scheduler::ReadyToRun (Thread *thread)
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
	thread->setStatus(READY);
    thread->setReady(kernel->stats->totalTicks);
    
	//cout << "Thread " <<  thread->getID() << " Process Ready " << kernel->stats->totalTicks << endl;
    //Aging();
    if(thread->getPriority()>=100){
		readyList_SJF->Insert(thread);
        cout << "Tick " << kernel->stats->totalTicks << ": Thread " <<thread->getID()<< " is inserted into queue L1. " << endl;
    }else if(thread->getPriority()>=50){
        readyList->Insert(thread);
        cout << "Tick " << kernel->stats->totalTicks << ": Thread " <<thread->getID()<< " is inserted into queue L2. " << endl;

    }
    else if(thread->getPriority()<50){
        readyList_RR->Append(thread);
        cout << "Tick " << kernel->stats->totalTicks << ": Thread " <<thread->getID()<< " is inserted into queue L3. " << endl;
    }
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------
//function edited
Thread *
Scheduler::FindNextToRun ()
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    Aging();
    if(readyList_SJF->IsEmpty()){	//if the highest priority L1 (SJF) is empty, then find L2 L3
		if(readyList->IsEmpty()){	//if L2 is empty, find L3
			if(readyList_RR->IsEmpty())	//if L3 is also empty
				return NULL;
			else {
				return readyList_RR->RemoveFront();
			}
		}
		else {
			return readyList->RemoveFront();
		}
    }
    else{ 
		return readyList_SJF->RemoveFront();
    }
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void
Scheduler::Run (Thread *nextThread, bool finishing)
{
    int exec;
    Thread *oldThread = kernel->currentThread;
    
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {	// mark that we need to delete current thread
         ASSERT(toBeDestroyed == NULL);
	 toBeDestroyed = oldThread;
    }
    
    if (oldThread->space != NULL) {	// if this thread is a user program,
        oldThread->SaveUserState(); 	// save the user's CPU registers
	oldThread->space->SaveState();
    }
    
    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    kernel->currentThread = nextThread;  // switch to the next thread
    nextThread->setStatus(RUNNING);      // nextThread is now running
// 
//    cout << "Thread " << kernel->currentThread->getID() << "\tProcessRunning\t" << kernel->stats->totalTicks << endl;    
    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());
    
    cout << "Tick " << kernel->stats->totalTicks << ": Thread " <<nextThread->getID()<< " is now selected for execution." << endl;
    //cout << "Tick " << kernel->stats->totalTicks << ": Thread " <<oldThread->getID()<< " is replaced, and it has executed " << kernel->stats->totalTicks - oldThread->getBurstStart() <<" ticks."<<endl;
    
    exec = oldThread->getTotalExe() + kernel->stats->totalTicks - oldThread->getBurstStart();
    oldThread -> setTotalExe(exec);
    cout << "Tick " << kernel->stats->totalTicks << ": Thread " <<oldThread->getID()<< " is replaced, and it has executed " << exec <<" ticks."<<endl;

    // This is a machine-dependent assembly language routine defined 
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    //burst time
    nextThread->setBurst( (kernel->stats->totalTicks - oldThread->getBurstStart() + oldThread->getBurst()) / 2.0 );
    nextThread->setBurstStart(kernel->stats->totalTicks);
    //

    SWITCH(oldThread, nextThread);

    // we're back, running oldThread
      
    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed();		// check if thread we were running
					// before this one has finished
					// and needs to be cleaned up
    
    if (oldThread->space != NULL) {	    // if there is an address space
        oldThread->RestoreUserState();     // to restore, do it.
	oldThread->space->RestoreState();
    }
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void
Scheduler::CheckToBeDestroyed()
{
    if (toBeDestroyed != NULL) {
        delete toBeDestroyed;
	toBeDestroyed = NULL;
    }
}
 
//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
Scheduler::Print()
{
    cout << "Ready list contents:\n";
    readyList->Apply(ThreadPrint);
}

//
//---------------------
//function added: aging for priority queuing
//---------------------
void
Scheduler::Aging()
{
    ListIterator<Thread *> *iter1=new ListIterator<Thread *>((List<Thread *>*)readyList_SJF);
    ListIterator<Thread *> *iter2=new ListIterator<Thread *>((List<Thread *>*)readyList);
    ListIterator<Thread *> *iter3=new ListIterator<Thread *>((List<Thread *>*)readyList_RR);
    
    while(!iter1->IsDone()){
        Thread* thread1=iter1->Item();
        iter1->Next();
        // int nowTicks1 = kernel->stats->totalTicks;
        if((kernel->stats->totalTicks - thread1->getReady()) >= 1500) {
            readyList_SJF->Remove(thread1);
            cout << "Tick " << kernel->stats->totalTicks << ": Thread " <<thread1->getID()<< " is removed from queue L1 " << endl;
            int pre1 = thread1->getPriority();
            if ((thread1->getPriority())<=149){
                thread1->setPriority(thread1->getPriority() + 10);
		thread1->setReady(kernel->stats->totalTicks);
                cout << "Tick " << kernel->stats->totalTicks << ": Thread " <<thread1->getID()<< " changes its priority from " << pre1 <<" to "<<thread1->getPriority()<< endl;
            }
            readyList_SJF->Insert(thread1);
            cout << "Tick " << kernel->stats->totalTicks << ": Thread " <<thread1->getID()<< " is inserted into queue L1 " << endl;
        }
    }
    while(!iter2->IsDone()){
        Thread* thread2=iter2->Item();
        iter2->Next();
        //int nowTicks2 = kernel->stats->totalTicks;
        if((kernel->stats->totalTicks - thread2->getReady()) >= 1500) {
            readyList->Remove(thread2);
            int pre2 = thread2->getPriority();
            cout << "Tick " << kernel->stats->totalTicks << ": Thread " <<thread2->getID()<< " is removed from queue L2" << endl;
            thread2->setPriority(thread2->getPriority() + 10);
	    thread2->setReady(kernel->stats->totalTicks);
            cout << "Tick " << kernel->stats->totalTicks << ": Thread " <<thread2->getID()<< " changes its priority from " <<pre2<<" to "<<thread2->getPriority()<< endl;
            
            if ((thread2->getPriority())>=100){
                readyList_SJF->Insert(thread2);
                cout << "Tick " << kernel->stats->totalTicks << ": Thread " <<thread2->getID()<< " is inserted into queue L1" << endl;
            }
            else{
                readyList->Insert(thread2);
                cout << "Tick " << kernel->stats->totalTicks << ": Thread " <<thread2->getID()<< " is inserted into queue L2" << endl;
            }
        }
    }
    while(!iter3->IsDone()){
        
        Thread* thread3=iter3->Item();
        iter3->Next();
        //int nowTicks3 = kernel->stats->totalTicks;
        if((kernel->stats->totalTicks - thread3->getReady()) >= 1500) {
            readyList_RR->Remove(thread3);
            int pre3 = thread3->getPriority();
            cout << "Tick " << kernel->stats->totalTicks << ": Thread " <<thread3->getID()<< " is removed from queue L3" << endl;
            thread3->setPriority(thread3->getPriority() + 10);
            thread3->setReady(kernel->stats->totalTicks);
	    cout << "Tick " << kernel->stats->totalTicks << ": Thread " <<thread3->getID()<< " changes its priority from " <<pre3<<" to "<<thread3->getPriority()<< endl;
            if ((thread3->getPriority())>=50){
                readyList->Insert(thread3);
                cout << "Tick " << kernel->stats->totalTicks << ": Thread " <<thread3->getID()<< " is inserted into queue L2" << endl;
            }
            else{
                readyList_RR->Append(thread3);
                cout << "Tick " << kernel->stats->totalTicks << ": Thread " <<thread3->getID()<< " is inserted into queue L3" << endl;
            }
        }
    }
}


//aging for pri
/*
void
Scheduler::aging(SortedList<Thread *>* list)
{ 
    ListIterator<Thread *> *iter = new ListIterator<Thread *>((List<Thread *>*)list);
    for (; !iter->IsDone(); iter->Next()) {
        Thread* thread = iter->Item();
        if(!list->IsInList(thread))break;
        list->Remove(thread);
	//cout << "Tick " << kernel->stats->totalTicks << " : Thread" <<thread->getID()<< " is removed from queue L2" << endl;
        if(kernel->stats->totalTicks - thread->getReady() >= 1500){
            thread->setReady(kernel->stats->totalTicks);
            cout << "Tick " << kernel->stats->totalTicks << " : Thread" <<thread->getID()<< " is removed from queue L2" << endl;
	    thread->setPriority(thread->getPriority()+10);
	}
        move(thread);
    }
}

////////////////
//aging for rr
void
Scheduler::aging2(List<Thread *>* list)
{
    ListIterator<Thread *> *iter = new ListIterator<Thread *>((List<Thread *>*)list);
    for (; !iter->IsDone(); iter->Next()) {
        Thread* thread = iter->Item();
        if(!list->IsInList(thread))break;
        list->Remove(thread);
        //cout << "Tick " << kernel->stats->totalTicks << " : Thread" <<thread->getID()<< " is removed from queue L3" << endl;
        if(kernel->stats->totalTicks - thread->getReady() >= 1500){
            cout << "Tick " << kernel->stats->totalTicks << " : Thread" <<thread->getID()<< " is removed from queue L3" << endl;
	    thread->setReady(kernel->stats->totalTicks);
            thread->setPriority(thread->getPriority()+10);
        }
        move(thread);
    }
}
///////////////////
//aging for sjf
void
Scheduler::aging3(SortedList<Thread *>* list)
{
    ListIterator<Thread *> *iter = new ListIterator<Thread *>((List<Thread *>*)list);
    for (; !iter->IsDone(); iter->Next()) {
        Thread* thread = iter->Item();
        if(!list->IsInList(thread))break;
        list->Remove(thread);
        //cout << "Tick " << kernel->stats->totalTicks << " : Thread" <<thread->getID()<< " is removed from queue L1" << endl;
	if(kernel->stats->totalTicks - thread->getReady() >= 1500){
	    cout << "Tick " << kernel->stats->totalTicks << " : Thread" <<thread->getID()<< " is removed from queue L1" << endl;
            thread->setReady(kernel->stats->totalTicks);
            thread->setPriority(thread->getPriority()+10);
        }
        move(thread);
    }
}

*/
//
//-------------------------------
//function added: move => move between queues
//------------------------------
/*
void
Scheduler::move(Thread* thread)  
{
    //L1:SJF
    if(thread->getPriority()>=100){
        cout<<"Tick "<<kernel->stats->totalTicks<<":Thread "<<thread->getID()<<" is inserted into queue L1"<<endl;
        readyList_SJF->Insert(thread);
    }
    //L2: priority
    else if (thread->getPriority()>=50){
        cout<<"Tick "<<kernel->stats->totalTicks<<":Thread "<<thread->getID()<<" is inserted into queue L2"<<endl;
        readyList->Insert(thread);
    }
    //L3: RR
    else {
        cout<<"Tick "<<kernel->stats->totalTicks<<":Thread "<<thread->getID()<<" is inserted into queue L3"<<endl;
        readyList_RR->Append(thread);
    }
}  
                              
*/
