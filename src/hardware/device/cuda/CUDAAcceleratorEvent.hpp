/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef CUDA_ACCELERATOR_EVENT_HPP
#define CUDA_ACCELERATOR_EVENT_HPP

#include "hardware/device/AcceleratorEvent.hpp"
#include "CUDAFunctions.hpp"
#include "CUDAStreamPool.hpp"

class CUDAAcceleratorEvent : public AcceleratorEvent
{
    private:
    CUDAStreamPool &_cudaPool;
    cudaEvent_t     _cudaEvent;
    cudaStream_t    _stream;
    public:

    CUDAAcceleratorEvent(std::function<void(AcceleratorEvent*)>& completion, cudaStream_t stream,  CUDAStreamPool& pool) :
        AcceleratorEvent(completion),
        _cudaPool(pool),
        _cudaEvent(_cudaPool.getCUDAEvent()),
        _stream(stream)
    {
    }

    float getMillisBetweenEvents(AcceleratorEvent &second)
    {
        //Only when there are TWO events that are CUDA events, we can use the vendor-specifig
        //functions.
        CUDAAcceleratorEvent* second_dc = dynamic_cast<CUDAAcceleratorEvent*>(&second);
        if(second_dc == nullptr)
            return AcceleratorEvent::getMillisBetweenEvents(second);

        return CUDAFunctions::getMillisBetweenEvents(_cudaEvent, second_dc->_cudaEvent);
    }

    bool vendorEventQuery()
    {
        return CUDAFunctions::cudaEventFinished(_cudaEvent);
    }

    void vendorEventRecord() override
    {
        CUDAFunctions::recordEvent(_cudaEvent, _stream);
    }

    ~CUDAAcceleratorEvent()
    {
        _cudaPool.releaseCUDAEvent(_cudaEvent);
    }
};

#endif //CUDA_ACCELERATOR_EVENT_HPP