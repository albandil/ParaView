/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkRedistributePolyData.cxx
  Language:  C++
  Date:      $Date$
  Version:   $Revision$


Copyright (c) 1993-2001 Ken Martin, Will Schroeder, Bill Lorensen 
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

 * Neither name of Ken Martin, Will Schroeder, or Bill Lorensen nor the names
   of any contributors may be used to endorse or promote products derived
   from this software without specific prior written permission.

 * Modified source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/
/*================================================================ ====
// This software and ancillary information known as vtk_ext (and
// herein called "SOFTWARE") is made available under the terms
// described below.  The SOFTWARE has been approved for release with
// associated LA_CC Number 99-44, granted by Los Alamos National
// Laboratory in July 1999.
//
// Unless otherwise indicated, this SOFTWARE has been authored by an
// employee or employees of the University of California, operator of
// the Los Alamos National Laboratory under Contract No. W-7405-ENG-36
// with the United States Department of Energy.
//
// The United States Government has rights to use, reproduce, and
// distribute this SOFTWARE.  The public may copy, distribute, prepare
// derivative works and publicly display this SOFTWARE without charge,
// provided that this Notice and any statement of authorship are
// reproduced on all copies.
//
// Neither the U. S. Government, the University of California, nor the
// Advanced Computing Laboratory makes any warranty, either express or
// implied, nor assumes any liability or responsibility for the use of
// this SOFTWARE.
//
// If SOFTWARE is modified to produce derivative works, such modified
// SOFTWARE should be clearly marked, so as not to confuse it with the
// version available from Los Alamos National Laboratory.
=========================================================================*/

#include "vtkRedistributePolyData.h"

#include "vtkMath.h"
#include "vtkObjectFactory.h"

#include "vtkPolyDataWriter.h"
#include "vtkUnsignedCharArray.h"
#include "vtkFloatArray.h"
#include "vtkCharArray.h"
#include "vtkIntArray.h"
#include "vtkUnsignedLongArray.h"
#include "vtkDoubleArray.h"
#include "vtkLongArray.h"
#include "vtkPolyData.h"

#include "vtkShortArray.h"
#include "vtkUnsignedIntArray.h"
#include "vtkUnsignedCharArray.h"
#include "vtkUnsignedLongArray.h"
#include "vtkUnsignedShortArray.h"
#include "vtkDataSetAttributes.h"

#include "vtkTimerLog.h"
#include "vtkString.h"

#include "vtkMultiProcessController.h"

vtkStandardNewMacro(vtkRedistributePolyData);
vtkCxxRevisionMacro(vtkRedistributePolyData, "1.10");

vtkCxxSetObjectMacro(vtkRedistributePolyData, Controller, 
                     vtkMultiProcessController);

#undef VTK_REDIST_DO_TIMING 
#define NUM_CELL_TYPES 4 

typedef struct {vtkTimerLog* timer; float time;} _TimerInfo;
_TimerInfo timerInfo8;

vtkRedistributePolyData::vtkRedistributePolyData()
{
  this->Controller = NULL;
  //this->Locator = vtkPointLocator::New();
  this->colorProc = 0;
}

vtkRedistributePolyData::~vtkRedistributePolyData()
{
  this->SetController(0);
}

void vtkRedistributePolyData::Execute()
{
#ifdef VTK_REDIST_DO_TIMING  
  vtkTimerLog* timer8 = vtkTimerLog::New();
  timerInfo8.timer = timer8;

  //timerInfo8.time = 0.;
  timerInfo8.timer->StartTimer();
#endif

  vtkPolyData *input = this->GetInput();
  vtkPolyData *output = this->GetOutput();

  int myId;
  if (!this->Controller)
    {
    this->SetController(vtkMultiProcessController::GetGlobalController());
    }

  if (!this->Controller)
    {
    vtkErrorMacro("need controller to redistribute cells");
    return;
    }
  myId = this->Controller->GetLocalProcessId();


  // ... make schedule of how many and where to ship polys ...

  this->Controller->Barrier();

#ifdef VTK_REDIST_DO_TIMING
  timerInfo8.Timer->StopTimer();
  timerInfo8.Time += timerInfo8.Timer->GetElapsedTime();
  if (myId==0)
    {
    vtkDebugMacro("barrier bef sched time = "<<timerInfo8.Time);
    }
  timerInfo8.Timer->StartTimer();
#endif

  vtkCommSched localSched;
  this->MakeSchedule ( &localSched ); 
  this->OrderSchedule ( &localSched);  // order schedule to avoid 
  // blocking problems later
  vtkIdType ***sendCellList = localSched.SendCellList; 
  vtkIdType **keepCellList  = localSched.KeepCellList; 
  int *sendTo  = localSched.SendTo;
  int *recFrom = localSched.ReceiveFrom; 
  int cntSend  = localSched.SendCount;
  int cntRec   = localSched.ReceiveCount;
  vtkIdType **sendNum = localSched.SendNumber; 
  vtkIdType **recNum  = localSched.ReceiveNumber; 
  vtkIdType *numCells = localSched.NumberOfCells;
  
#if VTK_REDIST_DO_TIMING
  timerInfo8.timer->StopTimer();
  timerInfo8.time += timerInfo8.timer->GetElapsedTime();
  if (myId==0)
    {
    vtkDebugMacro(<<"schedule time = "<<timerInfo8.time);
    }
  timerInfo8.timer->StartTimer();
#endif

  

// beginning of turned of bounds section (not needed)
#if 0
  // ... expand bounds on all processors to be the maximum on any processor ...

  this->Controller->Barrier();

#if VTK_REDIST_DO_TIMING
  timerInfo8.timer->StopTimer();
  timerInfo8.time += timerInfo8.timer->GetElapsedTime();
  if (myId==0)
    {
    vtkDegugMacro(<<"barrier bef bounds time = "<<timerInfo8.time);
    }
  timerInfo8.timer->StartTimer();
#endif


  int numProcs;
  numProcs = this->Controller->GetNumberOfProcesses();

  float *bounds = input->GetBounds(), *remoteBounds = bounds;

  for (id = 0; id < numProcs; id++)
    {
    // ... send out bounds to all the other processors ...
    if (id != myId)
      {
      this->Controller->Send(input->GetBounds(), 6, id, BOUNDS_TAG);
      }
    }
  
  for (id = 0; id < numProcs; id++)
    {
    if (id != myId)
      {
      // ... get remote bounds and expand bounds to include these ...
      this->Controller->Receive(remoteBounds, 6, id, BOUNDS_TAG);
      if (remoteBounds[0] < bounds[0]) bounds[0] = remoteBounds[0];
      if (remoteBounds[1] > bounds[1]) bounds[1] = remoteBounds[1];
      if (remoteBounds[2] < bounds[2]) bounds[2] = remoteBounds[2];
      if (remoteBounds[3] > bounds[3]) bounds[3] = remoteBounds[3];
      if (remoteBounds[4] < bounds[4]) bounds[4] = remoteBounds[4];
      if (remoteBounds[5] > bounds[5]) bounds[5] = remoteBounds[5];
      }
    }
  
#if VTK_REDIST_DO_TIMING
  timerInfo8.Timer->StopTimer();
  timerInfo8.Time += timerInfo8.Timer->GetElapsedTime();
  if (myId==0)
    {
    vtkDebugMacro(<<" bounds time = "<<timerInfo8.Time);
    }
  timerInfo8.Timer->StartTimer();
#endif

#endif   // end of turned of bounds section


  // ... allocate space and copy point and cell data attributes from input to 
  //   output ...

  output->GetPointData()->CopyAllocate(input->GetPointData());
  output->GetCellData()->CopyAllocate(input->GetCellData());
  
  // ... make sure output array info is initialized  ...

  int getArrayInfo = 0;
  int sendArrayInfo;

  if (input->GetPointData()->GetNumberOfArrays() == 0 ) 
    {
    // set request flag to true ...
    getArrayInfo = 1;
    }
  else
    {
    // ... set request flag to false because the array info isn't 
    //   needed ...
    getArrayInfo = 0;
    }

  // ... can get info from any processor that will be sending
  //  to this one because the remote processor must have the array
  //  info ...

  int type;
  int i;

  if (cntRec>0)
    {
    this->Controller->Send(&getArrayInfo, 1, recFrom[0], 997243);
    }

  // ... send false request flags to the rest of the processors 
  //   data is being received from ...

  int getArrayInfo2 = 0;
  for (i=1; i<cntRec; i++)
    {
    this->Controller->Send(&getArrayInfo2, 1, recFrom[i], 997243);
    }

  // ... loop over all processors data is being sent to and send 
  //   array information if it is needed ...

  for (i=0; i<cntSend; i++)
    {
    // ... get flag ...
    this->Controller->Receive(&sendArrayInfo, 1, sendTo[i], 997243);

    if (sendArrayInfo) 
      {
      this->SendCompleteArrays(sendTo[i]);
      }
    }


  // ... receive array info from first array in recFrom list ...

  if (cntRec>0 && getArrayInfo) 
    {
    this->CompleteArrays(recFrom[0]);
    }


  // ... copy remaining input cell data to output cell data ...

  vtkCellArray* inputCellArrays[NUM_CELL_TYPES];
  inputCellArrays[0] = input->GetVerts();
  inputCellArrays[1] = input->GetLines();
  inputCellArrays[2] = input->GetPolys();
  inputCellArrays[3] = input->GetStrips();

  vtkIdType inputNumCells[NUM_CELL_TYPES];
  vtkIdType origNumCells[NUM_CELL_TYPES]; 
  for (type=0; type<NUM_CELL_TYPES; type++) 
    {
    if (inputCellArrays[type])
      {
      inputNumCells[type] = inputCellArrays[type]->GetNumberOfCells();
      }
    else
      {
      inputNumCells[type] = 0;
      }
    origNumCells[type] = inputNumCells[type];
    }

  // check to see if number of cells is less than original and 
  // only copy that many or copy all of input cells if extra are 
  // added

  if (numCells[type]<origNumCells[type]) 
    {
    origNumCells[type] = numCells[type]; 
    }
#if VTK_REDIST_DO_TIMING
  timerInfo8.Timer->StopTimer();
  timerInfo8.Time += timerInfo8.Timer->GetElapsedTime();
  if (myId==0)
    {
    vtkDebugMacro(<<"copy alloctime = "<<timerInfo8.Time);
    }
  timerInfo8.Timer->StartTimer();
#endif


//sssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssss
// ... send cell and point sizes ...

  vtkIdType  prevStopCell[NUM_CELL_TYPES]; 
  vtkIdType  startCell[NUM_CELL_TYPES];
  vtkIdType  stopCell[NUM_CELL_TYPES];
  vtkIdType totalNumCellsToSend[NUM_CELL_TYPES];

// ... In certain cases cells may not only be copied on processor 
//   but the same cells may also be sent to another processor.  
//   Move the start sending cell back so that there are enough 
//   cells to send. ..


  for (type=0; type<NUM_CELL_TYPES;type++)
    {
    totalNumCellsToSend[type] = 0;
    for (i=0; i<cntSend; i++) { totalNumCellsToSend[type] += sendNum[type][i]; } 
    prevStopCell[type] = origNumCells[type] - 1;
    if (totalNumCellsToSend[type]+origNumCells[type] > inputNumCells[type])
      {
      prevStopCell[type] = inputNumCells[type] - totalNumCellsToSend[type] -1;
      }
    }

  vtkIdType *numPointsSend = new vtkIdType[cntSend];
  vtkIdType **cellArraySize = new vtkIdType*[cntSend];

  for (i=0; i<cntSend; i++)
    {
    cellArraySize[i] = new vtkIdType[NUM_CELL_TYPES];

    for (type=0; type<NUM_CELL_TYPES; type++)
      {
      startCell[type] = prevStopCell[type]+1;
      stopCell[type] = startCell[type]+sendNum[type][i]-1;
      prevStopCell[type] = stopCell[type];
      }
    this->SendCellSizes (startCell, stopCell, input, sendTo[i], 
                         numPointsSend[i], cellArraySize[i], 
                         sendCellList[i]);

    } // end of list of processors to send to

//ssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssss
//aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa

// ... allocate memory before receiving data ...

#if (DO_TIMING==1) 
  timerInfo8.timer->StopTimer();
  timerInfo8.time += timerInfo8.timer->GetElapsedTime();
  if (myId==0)
    {
    vtkDebugMacro(<<"send sizes time = "<<timerInfo8.time);
    }
  timerInfo8.timer->StartTimer();
#endif

// ... find memory requirements for on processor copy ...
  vtkIdType numPointsOnProc = 0;
  vtkIdType numCellPtsOnProc[NUM_CELL_TYPES];
  this->FindMemReq(origNumCells, input, numPointsOnProc, 
                   numCellPtsOnProc);

#if VTK_REDIST_DO_TIMING
  timerInfo8.timer->StopTimer();
  timerInfo8.time += timerInfo8.timer->GetElapsedTime();
  if (myId==0)
    {
    vtkDebugMacro(<<"mem req time = "<<timerInfo8.time);
    }
  timerInfo8.timer->StartTimer();
#endif

  vtkIdType* numPointsRec = new vtkIdType[cntRec];
  vtkIdType** cellptCntr = new vtkIdType*[cntRec];
  for (i=0; i<cntRec; i++)
    {
    cellptCntr[i] = new vtkIdType[NUM_CELL_TYPES];
    }

  for (i=0; i<cntRec; i++)
    {
    this->Controller->Receive((vtkIdType*)cellptCntr[i], 
                              NUM_CELL_TYPES, recFrom[i],
                              CELL_CNT_TAG);
    this->Controller->Receive((vtkIdType*)&numPointsRec[i], 1, 
                              recFrom[i],
                              POINTS_SIZE_TAG);
    }

  vtkCellData* outputCellData   = output->GetCellData();
  vtkPointData* outputPointData = output->GetPointData();

  this->AllocateCellDataArrays (outputCellData, recNum, cntRec, 
                                origNumCells );
  this->AllocatePointDataArrays (outputPointData, numPointsRec, 
                                 cntRec, numPointsOnProc);

  vtkIdType totalNumPoints = numPointsOnProc;
  vtkIdType totalNumCells[NUM_CELL_TYPES];
  vtkIdType totalNumCellPts[NUM_CELL_TYPES];

  for (type=0; type<NUM_CELL_TYPES; type++)
    {
    totalNumCells[type] = origNumCells[type];
    totalNumCellPts[type] = numCellPtsOnProc[type];
    }

  for (i=0; i<cntRec; i++)
    {
    totalNumPoints += numPointsRec[i];
    for (type=0; type<NUM_CELL_TYPES; type++)
      {
      totalNumCells[type] += recNum[type][i];
      totalNumCellPts[type] += cellptCntr[i][type];
      }
    }

  vtkPoints *outputPoints = vtkPoints::New();
  outputPoints->SetNumberOfPoints(totalNumPoints);

  vtkCellArray *outputVerts = NULL;
  vtkCellArray *outputLines = NULL;
  vtkCellArray *outputPolys = NULL;
  vtkCellArray *outputStrips = NULL;

  if (inputCellArrays[0]) { outputVerts = vtkCellArray::New();  }
  if (inputCellArrays[1]) { outputLines = vtkCellArray::New();  }
  if (inputCellArrays[2]) { outputPolys = vtkCellArray::New();  }
  if (inputCellArrays[3]) { outputStrips = vtkCellArray::New(); }

  vtkCellArray *outputCellArrays[NUM_CELL_TYPES];
  outputCellArrays[0] = outputVerts;
  outputCellArrays[1] = outputLines;
  outputCellArrays[2] = outputPolys;
  outputCellArrays[3] = outputStrips; 

  vtkIdType* ptr = 0; 
  for (type=0; type<NUM_CELL_TYPES; type ++)
    {
    if (totalNumCellPts[type] >0)
      {
      if (outputCellArrays[type])
        {
        ptr = outputCellArrays[type]->
          WritePointer(totalNumCells[type],totalNumCellPts[type]);
        if (ptr == 0) 
          {
          vtkErrorMacro("Error: can't allocate points.");
          }
        }
      }
    }

  output->SetVerts(outputVerts);
  output->SetLines(outputLines);
  output->SetPolys(outputPolys);
  output->SetStrips(outputStrips);

  output->SetPoints(outputPoints);

  //aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
  // ... Copy cells from input to output ...
  this->CopyCells(origNumCells, input, output, keepCellList);

#if VTK_REDIST_DO_TIMING
  timerInfo8.Timer->StopTimer();
  timerInfo8.Time += timerInfo8.Timer->GetElapsedTime();
  if (myId==0)
    {
    vtkDebugMacro("copy cells time = "<<timerInfo8.Time);
    }
  timerInfo8.Timer->StartTimer();
#endif

//eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee

  // ... first exchange cells between processors.  Do this by 
  //  receiving first if this processor number is less than the 
  //  one it is exchanging with else send first ...
  
  vtkIdType prevStopCellRec[NUM_CELL_TYPES];
  vtkIdType prevStopCellSend[NUM_CELL_TYPES];

  vtkIdType  prevNumPointsRec = numPointsOnProc;

  vtkIdType  prevCellptCntrRec[NUM_CELL_TYPES];

  for (type=0; type<NUM_CELL_TYPES; type++)
    {
    prevStopCellRec[type] = origNumCells[type] - 1;
    prevStopCellSend[type] = origNumCells[type] - 1;
    prevCellptCntrRec[type] = numCellPtsOnProc[type];

    if (totalNumCellsToSend[type]+origNumCells[type] > 
        inputNumCells[type])
      {
      prevStopCellSend[type] = inputNumCells[type] - 
        totalNumCellsToSend[type] -1;
      }
    }


  int finished = 0;
  int procRec,procSend;
  int rcntr=0;
  int scntr=0;
  int receiving;

  while (!finished && (cntRec>0 || cntSend>0))
    {
    if (rcntr<cntRec)
      {
      procRec = recFrom[rcntr];
      }
    else
      {
      procRec = 99999;
      }
    if (scntr<cntSend)
      {
      procSend = sendTo[scntr];
      }
    else
      {
      procSend = 99999;
      }

    receiving = 0;

    // ... send or receive the smallest processor number next 
    //   (will send if receive == 0) ...
    if (procRec<procSend)
      {
      receiving = 1;
      }
    else if (procRec==procSend)
      {
      // ... an exchange between 2 prcessors ...
      if (myId < procRec) 
        {
        receiving = 1;
        }
      }

    if (receiving)
      {
      for (type=0; type<NUM_CELL_TYPES; type++)
        {
        startCell[type] = prevStopCellRec[type]+1;
        stopCell[type] = startCell[type]+recNum[type][rcntr]-1;
        }

      this->ReceiveCells (startCell, stopCell, output, 
                          recFrom[rcntr], prevCellptCntrRec, 
                          cellptCntr[rcntr], prevNumPointsRec, 
                          numPointsRec[rcntr]);

      prevNumPointsRec += numPointsRec[rcntr];
      for (type=0; type<NUM_CELL_TYPES; type++)
        {
        prevCellptCntrRec[type] += cellptCntr[rcntr][type];
        prevStopCellRec[type] = stopCell[type];
        }
      rcntr++;
      }
    else
      {
      // ... sending ...
      if (sendCellList == NULL)
        {
        for (type=0; type<NUM_CELL_TYPES; type++)
          {
          startCell[type] = prevStopCellSend[type]+1;
          stopCell[type] = startCell[type]+sendNum[type][scntr]-1;
          }
        this->SendCells (startCell, stopCell, input, output, 
                         sendTo[scntr], numPointsSend[scntr], 
                         cellArraySize[scntr], NULL);
        }
      else
        {
        for (type=0; type<NUM_CELL_TYPES; type++)
          {
          startCell[type] = 0;
          stopCell[type] = sendNum[type][scntr]-1;
          }
        this->SendCells (startCell, stopCell, input, output, 
                         sendTo[scntr], numPointsSend[scntr], 
                         cellArraySize[scntr], sendCellList[scntr]);
        }

      for (type=0; type<NUM_CELL_TYPES; type++)
        {
        prevStopCellSend[type] = stopCell[type];
        }

      scntr++;
      }
    
    if (scntr>=cntSend && rcntr>=cntRec) { finished = 1;}
    }
//eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee

#if VTK_REDIST_DO_TIMING
  timerInfo8.Timer->StopTimer();
  timerInfo8.Time += timerInfo8.Timer->GetElapsedTime();
  if (myId==0)
    {
    vtkDebugMacro("send/rec (at end) time = "<<timerInfo8.Time);
    }
#endif
}

//*****************************************************************
void vtkRedistributePolyData::PrintSelf(ostream& os, vtkIndent indent)
{
  vtkPolyDataToPolyDataFilter::PrintSelf(os,indent);

  os << indent << "Controller (" << this->Controller << ")\n";
}


//*****************************************************************
void vtkRedistributePolyData::MakeSchedule ( vtkCommSched* localSched)

{
//*****************************************************************
// purpose: This routine sets up a schedule that does nothing.
//
//*****************************************************************

  // get total number of polys and figure out how many each 
  // processor should have

  vtkPolyData *input = this->GetInput();

  vtkCellArray *cellArrays[NUM_CELL_TYPES];
  cellArrays[0] = input->GetVerts();
  cellArrays[1] = input->GetLines();
  cellArrays[2] = input->GetPolys();
  cellArrays[3] = input->GetStrips();

  // ... initialize the local schedule to return ...
  int type;
  localSched->NumberOfCells = new vtkIdType[NUM_CELL_TYPES];
  for (type=0; type<NUM_CELL_TYPES; type++)
    {
    if (cellArrays[type])
      {
      localSched->NumberOfCells[type] = cellArrays[type]->
        GetNumberOfCells();
      }
    else
      {
      localSched->NumberOfCells[type] = 0;
      }
    }

  localSched->SendCount  = 0;
  localSched->ReceiveCount   = 0;
  localSched->SendTo   = NULL;
  localSched->SendNumber  = NULL;
  localSched->ReceiveFrom  = NULL;
  localSched->ReceiveNumber   = NULL;
  localSched->SendCellList = NULL;
  localSched->KeepCellList = NULL;

}
//*****************************************************************
void vtkRedistributePolyData::OrderSchedule ( vtkCommSched* localSched)

{
  vtkIdType ***sendCellList = localSched->SendCellList; 
  vtkIdType **sendNum = localSched->SendNumber; 
  vtkIdType **recNum  = localSched->ReceiveNumber; 
  int *sendTo  = localSched->SendTo;
  int *recFrom = localSched->ReceiveFrom; 
  int cntSend  = localSched->SendCount;
  int cntRec   = localSched->ReceiveCount;
  
  // ... first find number of exchanges where a processor both 
  //  sends and receives from the same processor ...

  int i,j;
  int* order;
  int temp;
  int tempid;
  vtkIdType* templist;
  int temporder;
  int type;


  // ... first order sends and then receives to avoid blocking 
  //  problems later ...
 
  int outoforder;  // flag to determine if schedule is out of order
  if (cntSend>0)
    {
    outoforder=0;
    order = new int[cntSend];
    for (i = 0; i<cntSend; i++) { order[i] = i;} 
    for (i = 0; i<cntSend; i++) 
      {
      for (j = i+1; j<cntSend; j++) 
        {
        if (sendTo[i] > sendTo[j])
          {
          temp = order[i];
          order[i] = order[j];
          order[j] = temp;
          outoforder=1;
          }
        }
      }

    // ... now reorder the sends ...
    if (outoforder)
      {
      for (i = 0; i<cntSend; i++) 
        {
        while (order[i] != i)
          {
          temporder = order[i];

          temp = sendTo[i];
          sendTo[i] = sendTo[temporder];
          sendTo[temporder] = temp;

          for (type=0; type<NUM_CELL_TYPES; type++)
            {
            tempid = sendNum[type][i];
            sendNum[type][i] = sendNum[type][temporder];
            sendNum[type][temporder] = tempid;
            }

          if (sendCellList != NULL)
            {
            for (type=0; type<NUM_CELL_TYPES; type++)
              {
              templist = sendCellList[i][type];
              sendCellList[i][type] = 
                sendCellList[temporder][type];
              sendCellList[temporder][type] = templist;
              }
            }

          temporder = order[i];
          order[i] = order[temporder];
          order[temporder] = temporder;
          } // end of while
        } // end of loop over cntSend
      } // end if outoforder
    delete [] order;
    } // end of cntSend>0

  if (cntRec>0)
    {
    outoforder=0;
    order = new int[cntRec];
    for (i = 0; i<cntRec; i++) { order[i] = i;} 
    for (i = 0; i<cntRec; i++) 
      {
      for (j = i+1; j<cntRec; j++) 
        {
        if (recFrom[i] > recFrom[j])
          {
          temp = order[i];
          order[i] = order[j];
          order[j] = temp;
          outoforder=1;
          }
        }
      }
    // ... now reorder the receives ...
    if (outoforder)
      {
      for (i = 0; i<cntRec; i++) 
        {
        while (order[i] != i)
          {
          temporder = order[i];

          temp = recFrom[i];
          recFrom[i] = recFrom[temporder];
          recFrom[temporder] = temp;

          for (type=0; type<NUM_CELL_TYPES; type++)
            {
            tempid = recNum[type][i];
            recNum[type][i] = recNum[type][temporder];
            recNum[type][temporder] = tempid;
            }

          temporder = order[i];
          order[i] = order[temporder];
          order[temporder] = temporder;
       
          } // end while
        } // end loop over cntRec
      } // end if outoforder
    delete [] order;
    } // end if cnrRec>0
}
//*****************************************************************
//*****************************************************************
// Copy the attribute data from one id to another. Make sure CopyAllocate() has// been invoked before using this method.
void vtkRedistributePolyData::CopyDataArrays
(vtkDataSetAttributes* fromPd, vtkDataSetAttributes* toPd,
 vtkIdType numToCopy, vtkIdType* fromId, int myId)
{

  vtkDataArray* DataFrom;
  vtkDataArray* DataTo;
  
  int numArrays = fromPd->GetNumberOfArrays();

  for (int i=0; i<numArrays; i++)
    {
    DataFrom = fromPd->GetArray(i);
    DataTo = toPd->GetArray(i);

    this->CopyArrays (DataFrom, DataTo, numToCopy, fromId, myId);
    } 

}
//*****************************************************************
// Copy the attribute data from one id to another. Make sure 
//   CopyAllocate() has been invoked before using this method.

void vtkRedistributePolyData::CopyCellBlockDataArrays
(vtkDataSetAttributes* fromPd, vtkDataSetAttributes* toPd,
 vtkIdType numToCopy, vtkIdType startCell, 
 int offset, int myId )
//*******************************************************************
{

  vtkDataArray* DataFrom;
  vtkDataArray* DataTo;

  int numArrays = fromPd->GetNumberOfArrays();

  for (int i=0; i<numArrays; i++)
    {
    DataFrom = fromPd->GetArray(i);
    DataTo = toPd->GetArray(i);

    this->CopyBlockArrays (DataFrom, DataTo, numToCopy, startCell, 
                           offset, myId);
    } 

}
//******************************************************************
void vtkRedistributePolyData::CopyArrays
(vtkDataArray* DataFrom, vtkDataArray* DataTo, 
 vtkIdType numToCopy, vtkIdType* fromId, int myId)
//******************************************************************
{
  char *cArrayFrom, *cArrayTo;
  int *iArrayFrom,  *iArrayTo;
  float *fArrayFrom, *fArrayTo;
  long *lArrayFrom,  *lArrayTo;
  vtkIdType *idArrayFrom,  *idArrayTo;
  unsigned long *ulArrayFrom, *ulArrayTo;
  unsigned char *ucArrayFrom, *ucArrayTo;
  double *dArrayFrom , *dArrayTo;

  vtkIdType i;
  int j;
  int numComps = DataFrom->GetNumberOfComponents();
  int dataType = DataFrom->GetDataType();

  switch (dataType)
    {
    case VTK_CHAR:
      cArrayFrom = ((vtkCharArray*)DataFrom)->GetPointer(0);
      cArrayTo = ((vtkCharArray*)DataTo)->GetPointer(0);
      for (i = 0; i < numToCopy; i++)
        {
        for (j = 0; j < numComps; j++)
          {
          cArrayTo[numComps*i+j]=
            cArrayFrom[numComps*fromId[i]+j];
          }
        
        }

      break;

    case VTK_UNSIGNED_CHAR:
      ucArrayFrom = ((vtkUnsignedCharArray*)DataFrom)->
        GetPointer(0);
      ucArrayTo = ((vtkUnsignedCharArray*)DataTo)->GetPointer(0);
      for (i = 0; i < numToCopy; i++)
        {
        for (j = 0; j < numComps; j++)
          {
          ucArrayTo[numComps*i+j]=
            ucArrayFrom[numComps*fromId[i]+j];
          }
        }

      break;

    case VTK_INT:
      iArrayFrom = ((vtkIntArray*)DataFrom)->GetPointer(0);
      iArrayTo = ((vtkIntArray*)DataTo)->GetPointer(0);
      for (i = 0; i < numToCopy; i++)
        {
        for (j = 0; j < numComps; j++)
          {
          iArrayTo[numComps*i+j]=
            iArrayFrom[numComps*fromId[i]+j];
          }
        }

      break;

    case VTK_UNSIGNED_LONG:
      ulArrayFrom = ((vtkUnsignedLongArray*)DataFrom)->
        GetPointer(0);
      ulArrayTo = ((vtkUnsignedLongArray*)DataTo)->GetPointer(0);
      for (i = 0; i < numToCopy; i++)
        {
        for (j = 0; j < numComps; j++)
          {
          ulArrayTo[numComps*i+j]=
            ulArrayFrom[numComps*fromId[i]+j];
          }
        }
      
      break;

    case VTK_FLOAT:
      fArrayFrom = ((vtkFloatArray*)DataFrom)->GetPointer(0);
      fArrayTo = ((vtkFloatArray*)DataTo)->GetPointer(0);
      for (i = 0; i < numToCopy; i++)
        {
        for (j = 0; j < numComps; j++)
          {
          fArrayTo[numComps*i+j]=
            fArrayFrom[numComps*fromId[i]+j];
          }
        }
       
      break;

    case VTK_DOUBLE:
      dArrayFrom = ((vtkDoubleArray*)DataFrom)->GetPointer(0);
      dArrayTo = ((vtkDoubleArray*)DataTo)->GetPointer(0);
      if (!colorProc)
        {
        for (i = 0; i < numToCopy; i++)
          {
          for (j = 0; j < numComps; j++)
            {
            dArrayTo[numComps*i+j]=
              dArrayFrom[numComps*fromId[i]+j];
            }
          }
        }
      else
        {
        for (i = 0; i < numToCopy; i++)
          {
          for (j = 0; j < numComps; j++)
            dArrayTo[numComps*i+j]= myId;
          }
        }

      break;

    case VTK_LONG:
      lArrayFrom = ((vtkLongArray*)DataFrom)->GetPointer(0);
      lArrayTo = ((vtkLongArray*)DataTo)->GetPointer(0);
      for (i = 0; i < numToCopy; i++)
        {
        for (j = 0; j < numComps; j++)
          {
          lArrayTo[numComps*i+j]=
            lArrayFrom[numComps*fromId[i]+j];
          }
        }

      break;
     
    case VTK_ID_TYPE:
      idArrayFrom = ((vtkIdTypeArray*)DataFrom)->GetPointer(0);
      idArrayTo = ((vtkIdTypeArray*)DataTo)->GetPointer(0);
      for (i = 0; i < numToCopy; i++)
        {
        for (j = 0; j < numComps; j++)
          {
          idArrayTo[numComps*i+j]=
            idArrayFrom[numComps*fromId[i]+j];
          }
        }

      break;
       
    case VTK_BIT:
      vtkErrorMacro("VTK_BIT not allowed for copy");
      break;
    case VTK_UNSIGNED_SHORT:
      vtkErrorMacro("VTK_UNSIGNED_SHORT not allowed for copy")
        break;
    case VTK_SHORT:
      vtkErrorMacro("VTK_SHORT not allowed for copy");
      break;
    case VTK_UNSIGNED_INT:
      vtkErrorMacro("VTK_UNSIGNED_INT not allowed for copy");
      break;
    default:
      vtkErrorMacro("datatype = "<<dataType<<" not allowed for copy");
    }
}
//------------------------------------------------------------------
//******************************************************************
void vtkRedistributePolyData::CopyBlockArrays
(vtkDataArray* DataFrom, vtkDataArray* DataTo, 
 vtkIdType numToCopy, vtkIdType startCell, 
 vtkIdType offset, int myId)
//******************************************************************
{
  char *cArrayTo, *cArrayFrom;
  int *iArrayTo, *iArrayFrom;
  float *fArrayTo, *fArrayFrom;
  long *lArrayTo, *lArrayFrom;
  vtkIdType *idArrayTo, *idArrayFrom;
  unsigned long *ulArrayTo, *ulArrayFrom;
  unsigned char *ucArrayTo, *ucArrayFrom;
  double *dArrayTo, *dArrayFrom;

  int numComps = DataFrom->GetNumberOfComponents();
  int dataType = DataFrom->GetDataType();

  vtkIdType  start = numComps*startCell;
  vtkIdType  size = numToCopy*numComps;
  vtkIdType  stop = start + size;

  vtkIdType i;

  switch (dataType)
    {
    case VTK_CHAR:
      cArrayFrom = ((vtkCharArray*)DataFrom)->GetPointer(0);
      cArrayTo = ((vtkCharArray*)DataTo)->GetPointer(offset);
      for (i=start; i<stop; i++) { cArrayTo[i] = cArrayFrom[i]; }
      break;

    case VTK_UNSIGNED_CHAR:
      ucArrayFrom = ((vtkUnsignedCharArray*)DataFrom)->GetPointer(0);
      ucArrayTo = ((vtkUnsignedCharArray*)DataTo)->
        GetPointer(offset);
      for (i=start; i<stop; i++) { ucArrayTo[i] = ucArrayFrom[i]; }
      break;

    case VTK_INT:
      iArrayFrom = ((vtkIntArray*)DataFrom)->GetPointer(0);
      iArrayTo = ((vtkIntArray*)DataTo)->GetPointer(offset);
      for (i=start; i<stop; i++) { iArrayTo[i] = iArrayFrom[i]; }
      break;

    case VTK_UNSIGNED_LONG:
      ulArrayFrom = ((vtkUnsignedLongArray*)DataFrom)->GetPointer(0);
      ulArrayTo = ((vtkUnsignedLongArray*)DataTo)->
        GetPointer(offset);
      for (i=start; i<stop; i++) { ulArrayTo[i] = ulArrayFrom[i]; }
      break;

    case VTK_FLOAT:
      fArrayFrom = ((vtkFloatArray*)DataFrom)->GetPointer(0);
      fArrayTo = ((vtkFloatArray*)DataTo)->GetPointer(offset);
      for (i=start; i<stop; i++) { fArrayTo[i] = fArrayFrom[i]; }
      break;

    case VTK_DOUBLE:
      dArrayFrom = ((vtkDoubleArray*)DataFrom)->GetPointer(0);
      dArrayTo = ((vtkDoubleArray*)DataTo)->GetPointer(offset);
      if (!colorProc)
        for (i=start; i<stop; i++) { dArrayTo[i] = dArrayFrom[i]; }
      else
        for (i=start; i<stop; i++) { dArrayTo[i] = myId; }
      break;

    case VTK_LONG:
      lArrayFrom = ((vtkLongArray*)DataFrom)->GetPointer(0);
      lArrayTo = ((vtkLongArray*)DataTo)->GetPointer(offset);
      for (i=start; i<stop; i++) { lArrayTo[i] = lArrayFrom[i]; }
      break;
        
    case VTK_ID_TYPE:
      idArrayFrom = ((vtkIdTypeArray*)DataFrom)->GetPointer(0);
      idArrayTo = ((vtkIdTypeArray*)DataTo)->GetPointer(offset);
      for (i=start; i<stop; i++) { idArrayTo[i] = idArrayFrom[i]; }
      break;
        
    case VTK_BIT:
      vtkErrorMacro("VTK_BIT not allowed for copy");
      break;
    case VTK_UNSIGNED_SHORT:
      vtkErrorMacro("VTK_UNSIGNED_SHORT not allowed for copy");
      break;
    case VTK_SHORT:
      vtkErrorMacro("VTK_SHORT not allowed for copy");
      break;
    case VTK_UNSIGNED_INT:
      vtkErrorMacro("VTK_UNSIGNED_INT not allowed for copy");
      break;
    default:
      vtkErrorMacro
        ("datatype = "<<dataType<<" not allowed for copy");
    }
}
//*****************************************************************
//*****************************************************************
void vtkRedistributePolyData::CopyCells (vtkIdType* numCells, 
                                         vtkPolyData* input, 
                                         vtkPolyData* output, 
                                         vtkIdType** keepCellList)

//*****************************************************************
{
  // ... Copy initial subset of cells and points from input to 
  //   output.  This assumes that the cells will be copied from 
  //   the beginning of the list. ... 


  int myId = this->Controller->GetLocalProcessId();
  vtkIdType cellId,i;

  // ... Copy cell data attribute data (Scalars, Vectors, etc.)...
  vtkCellArray* cellArrays[NUM_CELL_TYPES];
  cellArrays[0] = input->GetVerts();
  cellArrays[1] = input->GetLines();
  cellArrays[2] = input->GetPolys();
  cellArrays[3] = input->GetStrips();

  // ... assume that if there are any arrays in the inputCelldata
  //  it is ordered verts, lines, polygons and strips so that
  //  the first cell in lines corresponds with cell number
  //  equal to the number of vert cells. ... 


  vtkIdType cellOffset = 0; 

  vtkCellData* inputCellData = input->GetCellData();
  vtkCellData* outputCellData = output->GetCellData();

  // ...Since fromId's is used to point to cell data where
  //  data from all of the different types of cells is
  //  combined, use an offset because cellId points
  // to cell locations for the individual type. ... 

  int type;
  for (type=0; type<NUM_CELL_TYPES; type++)
    {
    vtkIdType* fromIds = new vtkIdType [numCells[type]];
    if (keepCellList != NULL)
      {
      for (cellId = 0; cellId <numCells[type]; cellId++) 
        {
        fromIds[cellId] = keepCellList[type][cellId]+cellOffset;
        }
      }

    if (keepCellList == NULL)
      {
      vtkIdType startCell = 0;
      this->CopyCellBlockDataArrays (inputCellData, outputCellData, 
                                     numCells[type], startCell,
                                     cellOffset, myId);
      }
    else
      {
      this->CopyDataArrays (inputCellData, outputCellData, 
                            numCells[type], fromIds, myId);
      }
    vtkIdType inputNumCells = 0;
    if (cellArrays[type]) inputNumCells = 
                            cellArrays[type]->GetNumberOfCells();
    {
    cellOffset += inputNumCells;
    }
    delete [] fromIds;

    }

#if VTK_REDIST_DO_TIMING
  timerInfo8.Timer->StopTimer();
  timerInfo8.Time += timerInfo8.Timer->GetElapsedTime();
  if (myId==-7)
    {
    vtkDebugMacro("1st copy data arrays time = "<<timerInfo8.Time);
    }
  timerInfo8.Timer->StartTimer();
#endif


  // ... Now copy points and point data. ...

  vtkPoints *outputPoints = output->GetPoints();
  vtkFloatArray* outputPointsArray = 
    (vtkFloatArray*)(outputPoints->GetData());
  float* outputPointsArrayData = outputPointsArray->GetPointer(0);

  vtkPoints *inputPoints = input->GetPoints();
  vtkFloatArray* inputPointsArray = NULL;
  if (inputPoints != NULL)
    {
    inputPointsArray = (vtkFloatArray*)(inputPoints->GetData());
    }
  float* inputPointsArrayData = NULL;
  if (inputPointsArray != NULL) 
    {
    inputPointsArrayData = inputPointsArray->GetPointer(0);
    }

#if VTK_REDIST_DO_TIMING
  timerInfo8.Timer->StopTimer();
  timerInfo8.Time += timerInfo8.Timer->GetElapsedTime();
  if (myId==-7)
    {
    vtDebugMacro("alloc  pts time = "<<timerInfo8.Time)
      }
  timerInfo8.Timer->StartTimer();
#endif


  // ... Allocate maximum possible number of points (use total 
  //  from all of input) ... 

  vtkIdType numPointsMax = input->GetNumberOfPoints();
  vtkIdType* fromPtIds = new vtkIdType[numPointsMax];

  vtkIdType* usedIds = new vtkIdType[numPointsMax];
  for (i=0; i<numPointsMax;i++) { usedIds[i]=-1;}


  // ... Copy point Id's for all the points in the cell. ...

  vtkCellArray* inputCellArrays[NUM_CELL_TYPES];
  inputCellArrays[0] = input->GetVerts();
  inputCellArrays[1] = input->GetLines();
  inputCellArrays[2] = input->GetPolys();
  inputCellArrays[3] = input->GetStrips();
  
  vtkCellArray* outputCellArrays[NUM_CELL_TYPES];
  outputCellArrays[0] = output->GetVerts();
  outputCellArrays[1] = output->GetLines();
  outputCellArrays[2] = output->GetPolys();
  outputCellArrays[3] = output->GetStrips();
  
  vtkIdType pointIncr = 0;
  vtkIdType pointId; 
  vtkIdType npts;

  vtkIdType* inPtr;
  vtkIdType* ptr;
  
  for (type=0; type<NUM_CELL_TYPES; type++)
    {
    inPtr = inputCellArrays[type]->GetPointer();
    ptr = outputCellArrays[type]->GetPointer();

    // ... set output number of points to input number of points ...
    if (keepCellList == NULL)
      {
      for (cellId = 0; cellId < numCells[type]; cellId++)
        {
        // ... set output number of points to input number 
        //   of points ...
        npts=*inPtr++;
        *ptr++ = npts;
        for (i = 0; i < npts; i++)
          {
          pointId = *inPtr++;
          if (usedIds[pointId] == -1)
            {
            vtkIdType newPt = pointIncr;
            *ptr++ = newPt;
            usedIds[pointId] = newPt;
            fromPtIds[pointIncr] = pointId;
            pointIncr++;
            }
          else
            {
            // ... use new point id ...
            *ptr++ = usedIds[pointId];
            }
          } // end loop over npts
        } // end loop over numCells
      } // end if section where keepCellList is null
    else
      {
      vtkIdType prevCellId = 0;
      for (vtkIdType id = 0; id < numCells[type]; id++)
        {
        cellId = keepCellList[type][id];
        for (i=prevCellId; i<cellId; i++)
          {
          npts=*inPtr++;
          inPtr += npts;
          }
        prevCellId = cellId+1;
  
        npts=*inPtr++;
        *ptr++ = npts;
        for (i = 0; i < npts; i++)
          {
          pointId = *inPtr++;
          if (usedIds[pointId] == -1)
            {
            vtkIdType newPt = pointIncr;
            *ptr++ = newPt;
            usedIds[pointId] = newPt;
            fromPtIds[pointIncr] = pointId;
            pointIncr++;
            }
          else
            {
            // ... use new point id ...
            *ptr++ = usedIds[pointId];
            }
          } // end loop over npts
        } // end loop over cells
      } // end else statement for keepCellList
    } // end loop over type


#if VTK_REDIST_DO_TIMING
  timerInfo8.Timer->StopTimer();
  timerInfo8.Time += timerInfo8.Timer->GetElapsedTime();
  if (myId==-7)
    {
    vtkDebugMacro("copy pt ids time = "<<timerInfo8.Time);
    }
  timerInfo8.Timer->StartTimer();
#endif


  // ... Copy cell points. ...
  vtkIdType inLoc, outLoc;
  vtkIdType numPoints = pointIncr;
  int j;

  // ... copy x,y,z coordinates ...
  for (i=0; i<numPoints; i++)
    {
    inLoc = fromPtIds[i]*3;
    outLoc = i*3;
    for (j=0;j<3;j++) 
      {
      outputPointsArrayData[outLoc+j] = 
        inputPointsArrayData[inLoc+j];
      }
    }

#if VTK_REDIST_DO_TIMING
  timerInfo8.Timer->StopTimer();
  timerInfo8.Time += timerInfo8.Timer->GetElapsedTime();
  if (myId==-7)
    {
    vtkDebugMacro("copy pts time = "<<timerInfo8.Time);
    }
  timerInfo8.Timer->StartTimer();
#endif

  vtkPointData* inputPointData = input->GetPointData();
  vtkPointData* outputPointData = output->GetPointData();

  // ... copy point data arrays ...
  this->CopyDataArrays (inputPointData, outputPointData, numPoints, 
                        fromPtIds, myId );
  delete [] fromPtIds;

#if VTK_REDIST_DO_TIMING
  timerInfo8.Timer->StopTimer();
  timerInfo8.Time += timerInfo8.Timer->GetElapsedTime();
  if (myId==-7)
    {
    vtkDebugMacro("setting time = "<<timerInfo8.Time);
    }
  timerInfo8.Timer->StartTimer();
#endif

}

//*****************************************************************
void vtkRedistributePolyData::SendCellSizes 
(vtkIdType* startCell, vtkIdType* stopCell, 
 vtkPolyData* input, int sendTo, vtkIdType& numPoints, 
 vtkIdType* ptcntr, vtkIdType** sendCellList)

//*****************************************************************
{
  // ... send cells and point sizes without sending data ...

  vtkIdType cellId,i;
  vtkIdType numCells; 

  int myId = this->Controller->GetLocalProcessId();

  // ... Allocate maximum possible number of points (use total from
  //     all of input) ...

  vtkIdType numPointsMax = input->GetNumberOfPoints();
  vtkIdType* usedIds = new vtkIdType [numPointsMax];
  for (i=0; i<numPointsMax;i++) { usedIds[i]=-1;}


  // ... send point Id's for all the points in the cell. ...


  vtkIdType pointIncr = 0;
  vtkIdType pointId; 
  vtkIdType npts;
  vtkIdType* inPtr;

  vtkCellArray* cellArrays[NUM_CELL_TYPES];
  cellArrays[0] = input->GetVerts();
  cellArrays[1] = input->GetLines();
  cellArrays[2] = input->GetPolys();
  cellArrays[3] = input->GetStrips();


  int type;
  for (type=0; type<NUM_CELL_TYPES; type++)
    {
    if (cellArrays[type])
      {
      inPtr = cellArrays[type]->GetPointer();
      ptcntr[type] = 0; // counts the number of points stored in the
      // cell array plus includes the extra space 
      // for each cell that contains the number of 
      // points in that cell. 

      if (sendCellList == NULL)
        {
        // ... send cells in a block ...
        for (cellId = 0; cellId < startCell[type]; cellId++)
          {
          // ... increment pointers to get to correct starting 
          //  point ...
          npts=*inPtr++;
          inPtr+=npts;
          }
   
        for (cellId = startCell[type]; cellId <= stopCell[type]; 
             cellId++)
          {
          // ... set output number of points to input number of 
          //   points ...
          npts=*inPtr++;
          ptcntr[type]++;
          for (i = 0; i < npts; i++)
            {
            pointId = *inPtr++;
            if (usedIds[pointId] == -1) 
              { 
              usedIds[pointId] = pointIncr++; 
              }
            ptcntr[type]++;
            }
          }
        }
      else
        {
        // ... there is a specific list of cells to send ...

        vtkIdType prevCellId = 0;
        numCells = stopCell[type]-startCell[type]+1;
 
        for (vtkIdType id = 0; id < numCells; id++)
          {
       
          cellId = sendCellList[type][id];
          for (i = prevCellId; i<cellId ; i++)
            {
            // ... increment pointers to get to correct starting 
            // point ...
            npts=*inPtr++;
            inPtr+=npts;
            }
          prevCellId = cellId+1;

          // ... set output number of points to input number of 
          //   points ...

          npts=*inPtr++;
          ptcntr[type]++;

          for (i = 0; i < npts; i++)
            {
            pointId = *inPtr++;
            if (usedIds[pointId] == -1) 
              usedIds[pointId] = pointIncr++;
            {
            ptcntr[type]++;
            }
            }
          } // end loop over cells
        } // end if sendCellList
      } // end if cellArrays
    } // end loop over type

  // ... send sizes first (must be in this order to allocate for 
  //   receive)...

  this->Controller->Send((vtkIdType*)ptcntr, NUM_CELL_TYPES, sendTo, 
                         CELL_CNT_TAG);

  numPoints = pointIncr;
  this->Controller->Send((vtkIdType*)&numPoints, 1, sendTo,
                         POINTS_SIZE_TAG);

}
//*****************************************************************
void vtkRedistributePolyData::SendCells 
(vtkIdType* startCell, vtkIdType* stopCell,
 vtkPolyData* input, vtkPolyData* output, int sendTo, 
 vtkIdType& numPoints, vtkIdType* cellArraySize, 
 vtkIdType** sendCellList)

//*****************************************************************
{
  // ... send cells, points and associated data from cells in
  //     specified region ...

  vtkIdType cellId,i;

  int myId = this->Controller->GetLocalProcessId();

  // ... Allocate maximum possible number of points (use total from
  //     all of input) ...

  vtkIdType numPointsMax = input->GetNumberOfPoints();
  vtkIdType* fromPtIds = new vtkIdType[numPointsMax];

  vtkIdType* usedIds = new vtkIdType[numPointsMax];
  for (i=0; i<numPointsMax;i++) { usedIds[i]=-1; }


  // ... send point Id's for all the points in the cell. ...

  vtkCellArray* inputCellArrays[NUM_CELL_TYPES];
  inputCellArrays[0] = input->GetVerts();
  inputCellArrays[1] = input->GetLines();
  inputCellArrays[2] = input->GetPolys();
  inputCellArrays[3] = input->GetStrips();
  
  vtkIdType* ptr;
  vtkIdType ptcntr[NUM_CELL_TYPES];
  vtkIdType* ptrsav[NUM_CELL_TYPES];

  vtkIdType pointIncr = 0;
  vtkIdType pointId; 
  vtkIdType* inPtr;
  vtkIdType npts;

  int type;

  vtkIdType numCells[NUM_CELL_TYPES];
  for (type=0; type<NUM_CELL_TYPES; type++)
    {
    inPtr = inputCellArrays[type]->GetPointer();
    ptr = new vtkIdType[cellArraySize[type]];
    ptrsav[type] = ptr;
    ptcntr[type] = 0;
    numCells[type] = stopCell[type]-startCell[type]+1;

    // ... set output number of points to input number of points ...
    if (sendCellList == NULL)
      {
      // ... send cells in a block ...
      for (cellId = 0; cellId < startCell[type]; cellId++)
        {
        // ... increment pointers to get to correct starting point ...
        npts=*inPtr++;
        inPtr+=npts;
        }
   
      for (cellId = startCell[type]; cellId <= stopCell[type]; 
           cellId++)
        {
        npts=*inPtr++;
        *ptr++ = npts;
        ptcntr[type]++;
        for (i = 0; i < npts; i++)
          {
          pointId = *inPtr++;
          if (usedIds[pointId] == -1)
            {
            vtkIdType newPt = pointIncr;
            *ptr++ = newPt;
            ptcntr[type]++;
            usedIds[pointId] = newPt;
            fromPtIds[pointIncr] = pointId;
            pointIncr++;
            }
          else
            {
            // ... use new point id ...
            *ptr++ = usedIds[pointId];
            ptcntr[type]++;
            }
          } // end loop over npts
        } // end loop over cellId
      }
    else
      {
      // ... there is a specific list of cells to send ...

      vtkIdType prevCellId = 0;

      for (vtkIdType  id = 0; id < numCells[type]; id++)
        {
       
        cellId = sendCellList[type][id];
        for (i = prevCellId; i<cellId ; i++)
          {
          // ... increment pointers to get to correct starting 
          //   point ...
          npts=*inPtr++;
          inPtr+=npts;
          }
        prevCellId = cellId+1;

        npts=*inPtr++;
        *ptr++ = npts;
        ptcntr[type]++;

        for (i = 0; i < npts; i++)
          {
          pointId = *inPtr++;
          if (usedIds[pointId] == -1)
            {
            vtkIdType newPt = pointIncr;
            *ptr++ = newPt;
            ptcntr[type]++;
            usedIds[pointId] = newPt;
            fromPtIds[pointIncr] = pointId;
            pointIncr++;
            }
          else
            {
            // ... use new point id ...
            *ptr++ = usedIds[pointId];
            ptcntr[type]++;
            }
          } // end loop over npts
        } // end loop over numCells
      } // end else where sendCellList isn't null
    } // end of type loop

  if (numPoints != pointIncr)
    {
    vtkErrorMacro("numPoints="<<numPoints<<", pointIncr="<<pointIncr
                  <<", should be equal");
    }

  delete [] usedIds;



  // ... send cell data attribute data (Scalars, Vectors, etc.)...

  vtkCellData* inputCellData = input->GetCellData();
  vtkCellData* outputCellData = output->GetCellData();

  vtkIdType cellOffset = 0;
  vtkIdType inputNumCells;

  vtkIdType* fromIds;
  vtkIdType cnt = 0;
  for (type=0; type<NUM_CELL_TYPES; type++)
    {
    fromIds = new vtkIdType[numCells[type]];
    if (sendCellList != NULL)
      {
      for (cellId = startCell[type]; cellId <=stopCell[type]; 
           cellId++) 
        {
        fromIds[cnt]= sendCellList[cnt][type]+cellOffset;
        cnt++;
        }
      }

    inputNumCells = 0;
    if (inputCellArrays[type])
      inputNumCells = inputCellArrays[type]->GetNumberOfCells();
    {
    cellOffset += inputNumCells;
    }


    // ... output needed for flags only (assumes flags are the same 
    //   on all processors) ...

    int typetag = type;  //(typetag = type for cells, =5 for points)
    if (sendCellList == NULL)
      {
      this->SendCellBlockDataArrays (inputCellData, outputCellData, 
                                     numCells[type], sendTo, 
                                     startCell[type]+cellOffset, 
                                     typetag );
      }
    else
      {
      this->SendDataArrays (inputCellData, outputCellData, 
                            numCells[type],sendTo, fromIds, typetag);
      }
    delete [] fromIds; // this array was allocated above in 
    // this case
    }


  // ... Send points Id's in cells now ...

  for (type=0; type<NUM_CELL_TYPES; type++)
    {
    if (ptcntr[type]>0)
      {
      this->Controller->Send(ptrsav[type], ptcntr[type], sendTo, 
                             CELL_TAG+type);
      }
    }


  // ... Copy cell points. ...

  vtkPoints *inputPoints = input->GetPoints();
  vtkFloatArray* inputPointsArray 
    = (vtkFloatArray*)(inputPoints->GetData());
  float* inputPointsArrayData = inputPointsArray->GetPointer(0);

  float* outputPointsArrayData = new float[3*numPoints];

  // ... send x,y, z coordinates of points
  int j;
  vtkIdType inLoc, outLoc;
  for (i=0; i<numPoints; i++)
    {
    inLoc = fromPtIds[i]*3;
    outLoc = i*3;
    for (j=0;j<3;j++) 
      {
      outputPointsArrayData[outLoc+j] = inputPointsArrayData[inLoc+j];
      }
    }


  // ... Send points now ...

  this->Controller->Send(outputPointsArrayData, 3*numPoints, sendTo,
                         POINTS_TAG);


  // ... use output for flags only to avoid unneccessary sends ...
  vtkPointData* inputPointData = input->GetPointData();
  vtkPointData* outputPointData = output->GetPointData();

  int typetag = 5; //(typetag = 0 for cells + type, =5 for points)

  this->SendDataArrays (inputPointData, outputPointData, numPoints, 
                        sendTo, fromPtIds, typetag);
  delete [] fromPtIds;

}
//****************************************************************
void vtkRedistributePolyData::ReceiveCells
(vtkIdType* startCell, vtkIdType* stopCell,
 vtkPolyData* output, int recFrom,
 vtkIdType* prevCellptCntr, vtkIdType* cellptCntr,
 vtkIdType prevNumPoints, vtkIdType numPoints)

//*****************************************************************
{
  // ... send cells, points and associated data from cells in
  //     specified region ...

  vtkIdType cellId,i;

  // ... receive cell data attribute data (Scalars, Vectors, etc.)...

  vtkIdType cnt = 0;
  vtkIdType cellOffset= 0;

  vtkCellData* outputCellData = output->GetCellData();

  vtkCellArray* outputCellArrays[NUM_CELL_TYPES];
  outputCellArrays[0] = output->GetVerts();
  outputCellArrays[1] = output->GetLines();
  outputCellArrays[2] = output->GetPolys();
  outputCellArrays[3] = output->GetStrips();

  int type;
  for (type=0; type<NUM_CELL_TYPES; type++)
    {
    vtkIdType numCells = stopCell[type]-startCell[type]+1;
    vtkIdType* toIds = new vtkIdType[numCells];
    for (cellId = startCell[type]; cellId <=stopCell[type]; 
         cellId++) 
      {
      toIds[cnt++]= cellId + cellOffset;
      }

    int typetag = type; //(typetag = type for cells, =5 for points)
    this->ReceiveDataArrays (outputCellData, numCells, recFrom, 
                             toIds, typetag);
    delete [] toIds;

    vtkIdType outputNumCells = 0;
    if (outputCellArrays[type])
      {
      outputNumCells = outputCellArrays[type]->GetNumberOfCells();
      }
    cellOffset += outputNumCells;
    }


  // ... receive point Id's for all the points in the cell. ...

  vtkIdType* outPtr;
  for (type=0; type<NUM_CELL_TYPES; type++)
    {
    if (outputCellArrays[type])
      {
      outPtr = outputCellArrays[type]->GetPointer(); 
      outPtr+= prevCellptCntr[type];

      if (outPtr)
        {
        this->Controller->Receive((vtkIdType*)outPtr, 
                                  cellptCntr[type], 
                                  recFrom, CELL_TAG+type);
        }

      // ... Fix pointId's (need to have offset added to represent 
      //   correct location ...

      for (cellId = startCell[type]; cellId <=stopCell[type]; 
           cellId++) 
        {
        vtkIdType npts=*outPtr++;
        for (i = 0; i < npts; i++)
          {
          *outPtr+=prevNumPoints;
          outPtr++;
          }
        }
      }
    } // end loop over type
  

  // ... Receive points now ...

  vtkPoints *outputPoints = output->GetPoints();
  vtkFloatArray* outputPointsArray = 
    (vtkFloatArray*)(outputPoints->GetData());
  float* outputPointsArrayData = outputPointsArray->GetPointer(0);

  this->Controller->
    Receive(&outputPointsArrayData[prevNumPoints*3], 3*numPoints,
            recFrom, POINTS_TAG);


  // ... receive point attribute data ...
  vtkIdType* toPtIds = new vtkIdType[numPoints];
  for (i=0; i<numPoints; i++) { toPtIds[i] = prevNumPoints+i; }

  vtkPointData* outputPointData = output->GetPointData();
  int typetag = 5; //(typetag = type for cells, =5 for points)
  this->ReceiveDataArrays (outputPointData, numPoints, recFrom, 
                           toPtIds, typetag);
  delete [] toPtIds;

}
//*******************************************************************
// Allocate space for the attribute data expected from all id's.

void vtkRedistributePolyData::AllocatePointDataArrays
(vtkDataSetAttributes* toPd, vtkIdType* numPtsToCopy, 
 int cntRec, vtkIdType numPtsToCopyOnProc)
{
  vtkIdType numPtsToCopyTotal = numPtsToCopyOnProc;
  int id;
  for (id=0;id<cntRec;id++) numPtsToCopyTotal += numPtsToCopy[id];
   

  // ... Use WritePointer to allocate memory because it copies 
  //   existing data and only allocates if necessary. ...

  vtkDataArray* Data;
  int numArrays = toPd->GetNumberOfArrays();

  for (int i=0; i<numArrays; i++)
    {
    Data = toPd->GetArray(i);

    this->AllocateArrays (Data, numPtsToCopyTotal );
    } 
}
//*******************************************************************
// Allocate space for the attribute data expected from all id's.

void vtkRedistributePolyData::AllocateCellDataArrays
(vtkDataSetAttributes* toPd, vtkIdType** numCellsToCopy, 
 int cntRec, vtkIdType* numCellsToCopyOnProc)
{

  int type;
  vtkIdType numCellsToCopyTotal = 0;
  for (type=0; type<NUM_CELL_TYPES; type++)
    {
    numCellsToCopyTotal += numCellsToCopyOnProc[type];

    int id;
    for (id=0;id<cntRec;id++) 
      {
      numCellsToCopyTotal += numCellsToCopy[type][id];
      }
    }
   

  vtkDataArray* Data;
  int numArrays = toPd->GetNumberOfArrays();

  for (int i=0; i<numArrays; i++)
    {
    Data = toPd->GetArray(i);

    this->AllocateArrays (Data, numCellsToCopyTotal );
    } 
//zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz


}
//****************************************************************
void vtkRedistributePolyData::AllocateArrays
(vtkDataArray* Data, vtkIdType numToCopyTotal )
//****************************************************************
{
  int dataType = Data->GetDataType();
  int numComp = Data->GetNumberOfComponents();

  if (numToCopyTotal >0)
    {
    switch (dataType)
      {
      case VTK_CHAR:

        if (((vtkCharArray*)Data)-> 
            WritePointer(0,numToCopyTotal*numComp) ==0)
          {
          vtkErrorMacro("Error: can't alloc mem for data array");
          }
        break;

      case VTK_UNSIGNED_CHAR:

        if (((vtkUnsignedCharArray*)Data)-> 
            WritePointer(0,numToCopyTotal*numComp) ==0)
          {
          vtkErrorMacro("Error: can't alloc mem for data array");
          }
        break;

      case VTK_INT:

        if (((vtkIntArray*)Data)->
            WritePointer(0,numToCopyTotal*numComp) ==0)
          {
          vtkErrorMacro("Error: can't alloc mem for data array");
          }
        break;

      case VTK_UNSIGNED_LONG:

        if (((vtkUnsignedLongArray*)Data)->
            WritePointer(0,numToCopyTotal*numComp) ==0)
          {
          vtkErrorMacro("Error: can't alloc mem for data array");
          }
        break;

      case VTK_FLOAT:

        if (((vtkFloatArray*)Data)->
            WritePointer(0,numToCopyTotal*numComp) ==0)
          {
          vtkErrorMacro("Error: can't alloc mem for data array");
          }
        break;

      case VTK_DOUBLE:

        if (((vtkDoubleArray*)Data)->
            WritePointer(0,numToCopyTotal*numComp) ==0)
          {
          vtkErrorMacro("Error: can't alloc mem for data array");
          }
        break;

      case VTK_LONG:

        if (((vtkLongArray*)Data)->
            WritePointer(0,numToCopyTotal*numComp) ==0)
          {
          vtkErrorMacro("Error: can't alloc mem for data array");
          }
        break;
        
      case VTK_ID_TYPE:

        if (((vtkIdTypeArray*)Data)->
            WritePointer(0,numToCopyTotal*numComp) ==0)
          {
          vtkErrorMacro("Error: can't alloc mem for data array");
          }
        break;
        
      case VTK_BIT:
        vtkErrorMacro("VTK_BIT not allowed for Data Arrays");
        break;
      case VTK_UNSIGNED_SHORT:
        vtkErrorMacro
          ("VTK_UNSIGNED_SHORT not allowed for Data Arrays");
        break;
      case VTK_SHORT:
        vtkErrorMacro("VTK_SHORT not allowed for Data Arrays");
        break;
      case VTK_UNSIGNED_INT:
        vtkErrorMacro
          ("VTK_UNSIGNED_INT not allowed for Data Arrays");
        break;
      default:
        vtkErrorMacro
          ("datatype = "<<dataType<<" not allowed for Data Arrays"
            );
      } // end of switch
    } // end of if numToCopyTotal>0
}
//----------------------------------------------------------------------
//*****************************************************************
void vtkRedistributePolyData::FindMemReq
(vtkIdType* origNumCells, vtkPolyData* input, vtkIdType& numPoints,
 vtkIdType* numCellPts)

//*****************************************************************
{
  // ... count number of cellpoints, corresponding points and 
  //   number of cells ...
  vtkIdType cellId,i;

  // ... Allocate maximum possible number of points (use total from
  //     all of input) ...

  vtkIdType numPointsMax = input->GetNumberOfPoints();
  vtkIdType* usedIds = new vtkIdType[numPointsMax];
  for (i=0; i<numPointsMax;i++) { usedIds[i]=-1; }


  // ... count point Id's for all the points in the cell
  //     and number of points that will be stored ...

  vtkIdType pointId; 

  vtkCellArray* cellArrays[NUM_CELL_TYPES];
  cellArrays[0] = input->GetVerts();
  cellArrays[1] = input->GetLines();
  cellArrays[2] = input->GetPolys();
  cellArrays[3] = input->GetStrips();

  numPoints = 0;

  vtkIdType* inPtr;

  int type;
  for (type=0; type<NUM_CELL_TYPES; type++)
    {
    if (cellArrays[type])
      {
      inPtr = cellArrays[type]->GetPointer();
      numCellPts[type] = 0;
      for (cellId = 0; cellId < origNumCells[type]; cellId++)
        {
        vtkIdType npts=*inPtr++;
        numCellPts[type]++;
        numCellPts[type]+=npts;
        for (i = 0; i < npts; i++)
          {
          pointId = *inPtr++;
          if (usedIds[pointId] == -1)
            {
            vtkIdType newPt = numPoints;
            usedIds[pointId] = newPt;
            numPoints++;
            }
          }
        }
      }
    }

  delete [] usedIds;
}

//*****************************************************************
//*****************************************************************
// Copy the attribute data from one id to another. Make sure CopyAllocate() has// been invoked before using this method.
void vtkRedistributePolyData::SendDataArrays
(vtkDataSetAttributes* fromPd, vtkDataSetAttributes* toPd,
 vtkIdType numToCopy, int sendTo, vtkIdType* fromId, 
 int typetag)
{
  
  vtkDataArray* Data;
  int numArrays = fromPd->GetNumberOfArrays();

  // Note: sendTag is just mpi tag to keep sends seperate
  int sendTag; 

  for (int i=0; i<numArrays; i++)
    {
    Data = fromPd->GetArray(i);

    sendTag = 200+ 10 * i + typetag; // these tags should be unique
    SendArrays (Data, numToCopy, sendTo, fromId, sendTag);
    } 
}
//*****************************************************************
// Copy the attribute data from one id to another. Make sure 
// CopyAllocate() has// been invoked before using this method.

void vtkRedistributePolyData::SendCellBlockDataArrays
(vtkDataSetAttributes* fromPd, vtkDataSetAttributes* toPd,
 vtkIdType numToCopy, int sendTo, 
 vtkIdType startCell, int typetag )
//*******************************************************************
{

  vtkDataArray* Data;
  int numArrays = fromPd->GetNumberOfArrays();

  for (int i=0; i<numArrays; i++)
    {
    Data = fromPd->GetArray(i);

    int sendTag = 200+ 10 * i + typetag; // these tags should be 
    // unique
    this->SendBlockArrays (Data, numToCopy, sendTo, startCell, 
                           sendTag);
    } 
}
//******************************************************************
void vtkRedistributePolyData::SendArrays
(vtkDataArray* Data, vtkIdType numToCopy, int sendTo, 
 vtkIdType* fromId, int sendTag)
//******************************************************************
{
  char* sc;
  char *cArray;
  int *iArray, *si;
  float *fArray, *sf;
  long *lArray, *sl;
  vtkIdType *idArray, *sid;
  unsigned long *ulArray, *sul;
  unsigned char *ucArray, *suc;
  double *dArray, *sd;
  int dataSize;

  vtkIdType i;
  int j;
  int numComps = Data->GetNumberOfComponents();
  int dataType = Data->GetDataType();

  switch (dataType)
    {
    case VTK_CHAR:
      cArray = ((vtkCharArray*)Data)->GetPointer(0);
      sc = new char[numToCopy*numComps];
      for (i = 0; i < numToCopy; i++)
        {
        for (j = 0; j < numComps; j++)
          {
          sc[numComps*i+j] = cArray[numComps*fromId[i]+j];
          }
        }

      this->Controller->
        Send(sc, numToCopy*numComps, sendTo, sendTag);
      delete [] sc;
      break;

    case VTK_UNSIGNED_CHAR:
      ucArray = ((vtkUnsignedCharArray*)Data)->GetPointer(0);
      suc = new unsigned char[numToCopy*numComps];
      for (i = 0; i < numToCopy; i++)
        {
        for (j = 0; j < numComps; j++)
          {
          suc[numComps*i+j] = ucArray[numComps*fromId[i]+j];
          }
        }

      this->Controller->
        Send((char*)suc, numToCopy*numComps, sendTo, sendTag);
      delete [] suc;
      break;

    case VTK_INT:
      iArray = ((vtkIntArray*)Data)->GetPointer(0);
      si = new int[numToCopy*numComps];
      for (i = 0; i < numToCopy; i++)
        {
        for (j = 0; j < numComps; j++)
          {
          si[numComps*i+j] = iArray[numComps*fromId[i]+j];
          }
        }

      this->Controller->
        Send(si, numToCopy*numComps, sendTo, sendTag);
      delete [] si;
      break;

    case VTK_UNSIGNED_LONG:
      ulArray = ((vtkUnsignedLongArray*)Data)->GetPointer(0);
      sul = new unsigned long [numToCopy*numComps];
      for (i = 0; i < numToCopy; i++)
        {
        for (j = 0; j < numComps; j++)
          {
          sul[numComps*i+j] = ulArray[numComps*fromId[i]+j];
          }
        }
        
      this->Controller->
        Send(sul, numToCopy*numComps, sendTo, sendTag);
      delete [] sul;
      break;

    case VTK_FLOAT:
      fArray = ((vtkFloatArray*)Data)->GetPointer(0);
      sf = new float[numToCopy*numComps];
      for (i = 0; i < numToCopy; i++)
        {
        for (j = 0; j < numComps; j++)
          {
          sf[numComps*i+j] = fArray[numComps*fromId[i]+j];
          }
        }
      
      this->Controller->
        Send(sf, numToCopy*numComps, sendTo, sendTag);
      delete [] sf;
      break;

    case VTK_DOUBLE:
      dArray = ((vtkDoubleArray*)Data)->GetPointer(0);
      dataSize = sizeof(double);
      sc = (char*)new char[numToCopy*dataSize*numComps];
      sd = (double*)sc;
      for (i = 0; i < numToCopy; i++)
        {
        for (j = 0; j < numComps; j++)
          {
          sd[numComps*i+j] = dArray[numComps*fromId[i]+j];
          }
        }

      this->Controller->
        Send(sc, numToCopy*numComps*dataSize, sendTo, sendTag);
      delete [] sc;
      break;

    case VTK_LONG:
      lArray = ((vtkLongArray*)Data)->GetPointer(0);
      dataSize = sizeof(long);
      sc = (char*)new long[numToCopy*dataSize*numComps];
      sl = (long*)sc;
      for (i = 0; i < numToCopy; i++)
        {
        for (j = 0; j < numComps; j++)
          {
          sl[numComps*i+j] = lArray[numComps*fromId[i]+j];
          }
        }

      this->Controller->
        Send(sc, numToCopy*numComps*dataSize, sendTo, sendTag);
      delete [] sc;
      break;
        
    case VTK_ID_TYPE:
      idArray = ((vtkIdTypeArray*)Data)->GetPointer(0);
      dataSize = sizeof(vtkIdType);
      sc = (char*)new vtkIdType[numToCopy*dataSize*numComps];
      sid = (vtkIdType*)sc;
      for (i = 0; i < numToCopy; i++)
        {
        for (j = 0; j < numComps; j++)
          {
          sid[numComps*i+j] = idArray[numComps*fromId[i]+j];
          }
        }

      this->Controller->
        Send(sc, numToCopy*numComps*dataSize, sendTo, sendTag);
      delete [] sc;
      break;
      
    case VTK_BIT:
      vtkErrorMacro("VTK_BIT not allowed for send");
      break;
    case VTK_UNSIGNED_SHORT:
      vtkErrorMacro("VTK_UNSIGNED_SHORT not allowed for send");
      break;
    case VTK_SHORT:
      vtkErrorMacro("VTK_SHORT not allowed for send");
      break;
    case VTK_UNSIGNED_INT:
      vtkErrorMacro("VTK_UNSIGNED_INT not allowed for send");
      break;
    default:
      vtkErrorMacro
        ("datatype = "<<dataType<<" not allowed for send");
    }
}
//-----------------------------------------------------------------
//******************************************************************
void vtkRedistributePolyData::SendBlockArrays
(vtkDataArray* Data, vtkIdType numToCopy, int sendTo, 
 vtkIdType startCell, int sendTag)
//******************************************************************
{
  char *cArray;
  int *iArray;
  float *fArray;
  long *lArray;
  vtkIdType *idArray;
  unsigned long *ulArray;
  unsigned char *ucArray;
  double *dArray;
  int dataSize;

  int numComps = Data->GetNumberOfComponents();
  int dataType = Data->GetDataType();

  vtkIdType start = numComps*startCell;
  vtkIdType size = numToCopy*numComps;

  switch (dataType)
    {
    case VTK_CHAR:
      cArray = ((vtkCharArray*)Data)->GetPointer(0);
      this->Controller->
        Send((char*)&cArray[start], size, sendTo, sendTag);
      break;

    case VTK_UNSIGNED_CHAR:
      ucArray = ((vtkUnsignedCharArray*)Data)->GetPointer(0);
      this->Controller->
        Send((char*)&ucArray[start], size, sendTo, sendTag);
      break;

    case VTK_INT:
      iArray = ((vtkIntArray*)Data)->GetPointer(0);
      this->Controller->
        Send((int*)&iArray[start], size, sendTo, sendTag);
      break;

    case VTK_UNSIGNED_LONG:
      ulArray = ((vtkUnsignedLongArray*)Data)->GetPointer(0);
      this->Controller->
        Send((unsigned long*)&ulArray[start], size, sendTo, sendTag);
      break;

    case VTK_FLOAT:
      fArray = ((vtkFloatArray*)Data)->GetPointer(0);
      this->Controller->
        Send((float*)&fArray[start], size, sendTo, sendTag);
      break;

    case VTK_DOUBLE:
      dArray = ((vtkDoubleArray*)Data)->GetPointer(0);
      dataSize = sizeof(double);
      this->Controller->
        Send((char*)&dArray[start], size*dataSize, sendTo, sendTag);
      break;

    case VTK_LONG:
      lArray = ((vtkLongArray*)Data)->GetPointer(0);
      dataSize = sizeof(long);
      this->Controller->
        Send((char*)&lArray[start], size*dataSize, sendTo, sendTag);
      break;
        
    case VTK_ID_TYPE:
      idArray = ((vtkIdTypeArray*)Data)->GetPointer(0);
      dataSize = sizeof(vtkIdType);
      this->Controller->
        Send((char*)&idArray[start], size*dataSize, sendTo, sendTag);
      break;
        
    case VTK_BIT:
      vtkErrorMacro("VTK_BIT not allowed for send");
      break;
    case VTK_UNSIGNED_SHORT:
      vtkErrorMacro("VTK_UNSIGNED_SHORT not allowed for send");
      break;
    case VTK_SHORT:
      vtkErrorMacro("VTK_SHORT not allowed for send");
      break;
    case VTK_UNSIGNED_INT:
      vtkErrorMacro("VTK_UNSIGNED_INT not allowed for send");
      break;
    default:
      vtkErrorMacro
        ("datatype = "<<dataType<<" not allowed for send");
    }
}
//*****************************************************************
// ... Receive the attribute data from recFrom.  Call 
//   AllocateDataArrays before calling this ...

void vtkRedistributePolyData::ReceiveDataArrays
(vtkDataSetAttributes* toPd, vtkIdType numToCopy, 
 int recFrom, vtkIdType* toId, int typetag)
{

  // ... this assumes that memory has been allocated already, this is
  //     helpful to avoid repeatedly resizing ...
  
  vtkDataArray* Data;
  int numArrays = toPd->GetNumberOfArrays();

  // Note: recTag is just mpi tag to keep receives seperate
  int recTag; 

  for (int i=0; i<numArrays; i++)
    {
    Data = toPd->GetArray(i);

    recTag = 200+ 10 * i + typetag; // these tags should be unique
    this->ReceiveArrays (Data, numToCopy, recFrom, toId, recTag);
    } 

}
//*******************************************************************
void vtkRedistributePolyData::ReceiveArrays
(vtkDataArray* Data, vtkIdType numToCopy, int recFrom,
 vtkIdType* toId, int recTag)
//*******************************************************************
{
  char* sc;
  char *cArray;
  int *iArray, *si;
  float *fArray, *sf;
  long *lArray, *sl;
  vtkIdType *idArray, *sid;
  unsigned long *ulArray, *sul;
  unsigned char *ucArray, *suc;
  double *dArray, *sd;
  int dataSize;
  int numComps = Data->GetNumberOfComponents();
  int dataType = Data->GetDataType();

  vtkIdType i;
  int j;

  switch (dataType)
    {
    case VTK_CHAR:
      cArray = ((vtkCharArray*)Data)->GetPointer(0);
      sc = new char[numToCopy*numComps];

      this->Controller->
        Receive(sc, numToCopy*numComps, recFrom, recTag);
      for (i = 0; i < numToCopy; i++)
        {
        for (j = 0; j < numComps; j++)
          {
          cArray[toId[i]*numComps+j] = sc[numComps*i+j];
          }
        }

      delete [] sc;
      break;

    case VTK_UNSIGNED_CHAR:
      ucArray = ((vtkUnsignedCharArray*)Data)->GetPointer(0);
      suc = new unsigned char[numToCopy*numComps];

      this->Controller->
        Receive((char*)suc, numToCopy*numComps, recFrom, recTag);
      for (i = 0; i < numToCopy; i++)
        {
        for (j = 0; j < numComps; j++)
          {
          ucArray[toId[i]*numComps+j] = suc[numComps*i+j];
          }
        }

      delete [] suc;
      break;

    case VTK_INT:
      iArray = ((vtkIntArray*)Data)->GetPointer(0);
      si = new int[numToCopy*numComps];

      this->Controller->
        Receive(si, numToCopy*numComps, recFrom, recTag);
      for (i = 0; i < numToCopy; i++)
        {
        for (j = 0; j < numComps; j++)
          {
          iArray[toId[i]*numComps+j] = si[numComps*i+j];
          }
        }

      delete [] si;
      break;

    case VTK_UNSIGNED_LONG:
      ulArray = 
        ((vtkUnsignedLongArray*)Data)->GetPointer(0);
      sul = new unsigned long [numToCopy*numComps];

      this->Controller->
        Receive(sul, numToCopy*numComps, recFrom, recTag);
      for (i = 0; i < numToCopy; i++)
        {
        for (j = 0; j < numComps; j++)
          {
          ulArray[toId[i]*numComps+j] = sul[numComps*i+j];
          }
        }
      
      delete [] sul;
      break;

    case VTK_FLOAT:
      fArray = ((vtkFloatArray*)Data)->GetPointer(0);
      sf = new float[numToCopy*numComps];

      this->Controller->
        Receive(sf, numToCopy*numComps, recFrom, recTag);
      for (i = 0; i < numToCopy; i++)
        {
        for (j = 0; j < numComps; j++)
          {
          fArray[toId[i]*numComps+j] = sf[numComps*i+j];
          }
        }
      
      delete [] sf;
      break;

    case VTK_DOUBLE:
      dArray = ((vtkDoubleArray*)Data)->GetPointer(0);
      dataSize = sizeof(double);
      sc = (char*)new char[numToCopy*numComps*dataSize];
      sd = (double*)sc;

      this->Controller->
        Receive(sc, numToCopy*numComps*dataSize, recFrom, recTag);
      if (!colorProc)
        {
        for (i = 0; i < numToCopy; i++)
          {
          for (j = 0; j < numComps; j++)
            {
            dArray[toId[i]*numComps+j] = sd[numComps*i+j];
            }
          }
        }
      else
        {
        for (i = 0; i < numToCopy; i++)
          {
          for (j = 0; j < numComps; j++)
            {
            dArray[toId[i]*numComps+j] = recFrom;
            }
          }
        }

      delete [] sc;
      break;

    case VTK_LONG:
      lArray = ((vtkLongArray*)Data)->GetPointer(0);
      dataSize = sizeof(long);
      sc = (char*)new long[numToCopy*numComps*dataSize];
      sl = (long*)sc;

      this->Controller->
        Receive(sc, numToCopy*numComps*dataSize, recFrom, recTag);
      for (i = 0; i < numToCopy; i++)
        {
        for (j = 0; j < numComps; j++)
          {
          lArray[toId[i]*numComps+j] = sl[numComps*i+j];
          }
        }

      delete [] sc;
      break;
      
    case VTK_ID_TYPE:
      idArray = ((vtkIdTypeArray*)Data)->GetPointer(0);
      dataSize = sizeof(vtkIdType);
      sc = (char*)new vtkIdType[numToCopy*numComps*dataSize];
      sid = (vtkIdType*)sc;

      this->Controller->
        Receive(sc, numToCopy*numComps*dataSize, recFrom, recTag);
      for (i = 0; i < numToCopy; i++)
        {
        for (j = 0; j < numComps; j++)
          {
          idArray[toId[i]*numComps+j] = sid[numComps*i+j];
          }
        }

      delete [] sc;
      break;
      
    case VTK_BIT:
      vtkErrorMacro("VTK_BIT not allowed for receive");
      break;
    case VTK_UNSIGNED_SHORT:
      vtkErrorMacro("VTK_UNSIGNED_SHORT not allowed for receive");
      break;
    case VTK_SHORT:
      vtkErrorMacro("VTK_SHORT not allowed for receive");
      break;
    case VTK_UNSIGNED_INT:
      vtkErrorMacro("VTK_UNSIGNED_INT not allowed for receive");
      break;
    default:
      vtkErrorMacro
        ("datatype = "<<dataType<<" not allowed for receive");
    }
}

//--------------------------------------------------------------------
void vtkRedistributePolyData::CompleteArrays(int recFrom)
{
  int j;

  int num;
  vtkDataArray *array;
  char *name;
  int nameLength;
  int type;
  int numComps;
  int index;
  int attributeType;
  int copyFlag;

  vtkPolyData* output = this->GetOutput();


  // First Point data.

  this->Controller->Receive(&num, 1, recFrom, 997244);
  for (j = 0; j < num; ++j)
    {
    this->Controller->Receive(&type, 1, recFrom, 997245);
    switch (type)
      {
      case VTK_INT:
        array = vtkIntArray::New();
        break;
      case VTK_FLOAT:
        array = vtkFloatArray::New();
        break;
      case VTK_DOUBLE:
        array = vtkDoubleArray::New();
        break;
      case VTK_CHAR:
        array = vtkCharArray::New();
        break;
      case VTK_LONG:
        array = vtkLongArray::New();
        break;
      case VTK_SHORT:
        array = vtkShortArray::New();
        break;
      case VTK_UNSIGNED_CHAR:
        array = vtkUnsignedCharArray::New();
        break;
      case VTK_UNSIGNED_INT:
        array = vtkUnsignedIntArray::New();
        break;
      case VTK_UNSIGNED_LONG:
        array = vtkUnsignedLongArray::New();
        break;
      case VTK_UNSIGNED_SHORT:
        array = vtkUnsignedShortArray::New();
        break;
      }
    this->Controller->Receive(&numComps, 1, recFrom, 997246);
    array->SetNumberOfComponents(numComps);
    this->Controller->Receive(&nameLength, 1, recFrom, 997247);
    name = new char[nameLength];
    this->Controller->Receive(name, nameLength, recFrom, 997248);
    array->SetName(name);
    delete [] name;

    index = output->GetPointData()->AddArray(array);

    this->Controller->Receive(&attributeType, 1, recFrom, 997249);
    this->Controller->Receive(&copyFlag, 1, recFrom, 997250);

    if (attributeType != -1 && copyFlag)
      {
      output->GetPointData()->
        SetActiveAttribute(index, attributeType);
      }

    array->Delete();
    } // end of loop over point arrays.


  // Next Cell data.

  this->Controller->Receive(&num, 1, recFrom, 997244);
  for (j = 0; j < num; ++j)
    {
    this->Controller->Receive(&type, 1, recFrom, 997245);
    switch (type)
      {
      case VTK_INT:
        array = vtkIntArray::New();
        break;
      case VTK_FLOAT:
        array = vtkFloatArray::New();
        break;
      case VTK_DOUBLE:
        array = vtkDoubleArray::New();
        break;
      case VTK_CHAR:
        array = vtkCharArray::New();
        break;
      case VTK_LONG:
        array = vtkLongArray::New();
        break;
      case VTK_SHORT:
        array = vtkShortArray::New();
        break;
      case VTK_UNSIGNED_CHAR:
        array = vtkUnsignedCharArray::New();
        break;
      case VTK_UNSIGNED_INT:
        array = vtkUnsignedIntArray::New();
        break;
      case VTK_UNSIGNED_LONG:
        array = vtkUnsignedLongArray::New();
        break;
      case VTK_UNSIGNED_SHORT:
        array = vtkUnsignedShortArray::New();
        break;
      }
    this->Controller->Receive(&numComps, 1, recFrom, 997246);
    array->SetNumberOfComponents(numComps);
    this->Controller->Receive(&nameLength, 1, recFrom, 997247);
    name = new char[nameLength];
    this->Controller->Receive(name, nameLength, recFrom, 997248);
    array->SetName(name);
    delete [] name;
    index = output->GetCellData()->AddArray(array);

    this->Controller->Receive(&attributeType, 1, recFrom, 997249);
    this->Controller->Receive(&copyFlag, 1, recFrom, 997250);

    if (attributeType != -1 && copyFlag)
      {
      output->GetCellData()->
        SetActiveAttribute(index, attributeType);
      }

    array->Delete();
    } // end of loop over cell arrays.
  
}



//-----------------------------------------------------------------------
void vtkRedistributePolyData::SendCompleteArrays (int sendTo)
{
  int num;
  int i;
  int type;
  int numComps;
  int nameLength;
  const char *name;
  vtkDataArray *array;
  int attributeType; 
  int copyFlag;

  vtkPolyData* input = this->GetInput();

  // First point data.
  num = input->GetPointData()->GetNumberOfArrays();
  this->Controller->Send(&num, 1, sendTo, 997244);
  for (i = 0; i < num; ++i)
    {
    array = input->GetPointData()->GetArray(i);
    type = array->GetDataType();

    this->Controller->Send(&type, 1, sendTo, 997245);
    numComps = array->GetNumberOfComponents();

    this->Controller->Send(&numComps, 1, sendTo, 997246);
    name = array->GetName();
    if (name == NULL)
      {
      name = "";
      }
    nameLength = strlen(name)+1;
    this->Controller->Send(&nameLength, 1, sendTo, 997247);
    // I am pretty sure that Send does not modify the string.
    this->Controller->Send(const_cast<char*>(name), nameLength, 
                           sendTo, 997248);

    attributeType = input->GetPointData()->IsArrayAnAttribute(i);
    copyFlag = -1;
    if (attributeType != -1) 
      {
      // ... Note: this would be much simpler if there was a 
      //    GetCopyAttributeFlag function or if the variable 
      //    wasn't protected. ...
      switch (attributeType)
        {
        case vtkDataSetAttributes::SCALARS:
          copyFlag = input->GetPointData()->GetCopyScalars();
          break;

        case vtkDataSetAttributes::VECTORS: 
          copyFlag = input->GetPointData()->GetCopyVectors(); 
          break;

        case vtkDataSetAttributes::NORMALS:
          copyFlag = input->GetPointData()->GetCopyNormals();
          break;

        case vtkDataSetAttributes::TCOORDS:
          copyFlag = input->GetPointData()->GetCopyTCoords();
          break;

        case vtkDataSetAttributes::TENSORS:
          copyFlag = input->GetPointData()->GetCopyTensors();
          break;

        default:
          copyFlag = 0;

        }
      }
    this->Controller->Send(&attributeType, 1, sendTo, 997249);
    this->Controller->Send(&copyFlag, 1, sendTo, 997250);

    }

  // Next cell data.
  num = input->GetCellData()->GetNumberOfArrays();
  this->Controller->Send(&num, 1, sendTo, 997244);
  for (i = 0; i < num; ++i)
    {
    array = input->GetCellData()->GetArray(i);
    type = array->GetDataType();

    this->Controller->Send(&type, 1, sendTo, 997245);
    numComps = array->GetNumberOfComponents();

    this->Controller->Send(&numComps, 1, sendTo, 997246);
    name = array->GetName();
    if (name == NULL)
      {
      name = "";
      }
    nameLength = strlen(name+1);
    this->Controller->Send(&nameLength, 1, sendTo, 997247);
    this->Controller->Send(const_cast<char*>(name), nameLength, 
                           sendTo, 997248);
    attributeType = input->GetCellData()->IsArrayAnAttribute(i);
    copyFlag = -1;
    if (attributeType != -1) 
      {
      // ... Note: this would be much simpler if there was a 
      //    GetCopyAttributeFlag function or if the variable 
      //    wasn't protected. ...
      switch (attributeType)
        {
        case vtkDataSetAttributes::SCALARS:
          copyFlag = input->GetCellData()->GetCopyScalars();
          break;

        case vtkDataSetAttributes::VECTORS: 
          copyFlag = input->GetCellData()->GetCopyVectors(); 
          break;

        case vtkDataSetAttributes::NORMALS:
          copyFlag = input->GetCellData()->GetCopyNormals();
          break;

        case vtkDataSetAttributes::TCOORDS:
          copyFlag = input->GetCellData()->GetCopyTCoords();
          break;

        case vtkDataSetAttributes::TENSORS:
          copyFlag = input->GetCellData()->GetCopyTensors();
          break;

        default:
          copyFlag = 0;

        }
      }
    this->Controller->Send(&attributeType, 1, sendTo, 997249);
    this->Controller->Send(&copyFlag, 1, sendTo, 997250);
    }
}


//=============================================================

vtkRedistributePolyData::vtkCommSched::vtkCommSched()
{
  // ... initalize a communication schedule to do nothing ...
  this->NumberOfCells = 0;
  this->SendCount  = 0;
  this->ReceiveCount   = 0;
  this->SendTo  = NULL;
  this->SendNumber = NULL;
  this->ReceiveFrom = NULL;
  this->ReceiveNumber  = NULL;
  this->SendCellList = NULL;
  this->KeepCellList = NULL;
}

//*****************************************************************
vtkRedistributePolyData::vtkCommSched::~vtkCommSched()
{
  delete [] this->SendTo;
  delete [] this->ReceiveFrom;
  int type;
  
  for (type=0; type<NUM_CELL_TYPES; type++)
    {
    if (this->SendNumber !=NULL) 
      {
      delete [] this->SendNumber[type];
      }
    if (this->ReceiveNumber !=NULL) 
      {
      delete [] this->ReceiveNumber[type];
      }

    if (this->SendCellList != NULL) 
      {
      for (int i=0; i<this->SendCount; i++) 
        {
        delete [] this->SendCellList[i][type];
        }
      }
    if (this->KeepCellList != NULL) 
      {
      delete [] this->KeepCellList[type];
      }
    }
  if (this->SendCellList != NULL) 
    {
    for (int i=0; i<this->SendCount; i++) 
      {
      delete [] this->SendCellList[i];
      }
    delete [] this->SendCellList;
    }

  delete [] this->SendNumber;
  delete [] this->ReceiveNumber;
  delete [] this->KeepCellList;
}
//*****************************************************************
