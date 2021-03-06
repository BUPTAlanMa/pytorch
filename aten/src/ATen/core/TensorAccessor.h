#pragma once

#include <cstddef>
#include <stdint.h>

namespace at {

// The PtrTraits argument to the TensorAccessor/PackedTensorAccessor
// is used to enable the __restrict__ keyword/modifier for the data
// passed to cuda.
template <typename T>
struct DefaultPtrTraits {
  typedef T* PtrType;
};

#ifdef __CUDACC__
template <typename T>
struct RestrictPtrTraits {
  typedef T* __restrict__ PtrType;
};
#endif

#ifndef AT_HOSTDEVICE
#ifdef __CUDACC__
#define AT_HOSTDEVICE __host__ __device__
#define AT_HOST __host__
#define AT_DEVICE __device__
#else
#define AT_HOSTDEVICE
#define AT_HOST
#define AT_DEVICE
#endif
#endif

// TensorAccessorBase and TensorAccessor are used for both CPU and CUDA tensors.
// For CUDA tensors it is used in device code (only). This means that we restrict ourselves
// to functions and types available there (e.g. IntList isn't).

// The PtrTraits argument is only relevant to cuda to support `__restrict__` pointers.
template<typename T, size_t N, template <typename U> class PtrTraits = DefaultPtrTraits>
class TensorAccessorBase {
public:
  typedef typename PtrTraits<T>::PtrType PtrType;

  AT_HOSTDEVICE TensorAccessorBase(PtrType data_, const int64_t * sizes_, const int64_t * strides_)
  : data_(data_), sizes_(sizes_), strides_(strides_) {}
  AT_HOST IntList sizes() const {
    return IntList(sizes_,N);
  }
  AT_HOST IntList strides() const {
    return IntList(strides_,N);
  }
  AT_HOSTDEVICE int64_t stride(int64_t i) const { return strides_[i]; }
  AT_HOSTDEVICE int64_t size(int64_t i) const { return sizes_[i]; }
  AT_HOSTDEVICE T *data() { return data_; }
  AT_HOSTDEVICE const T *data() const { return data_; }
protected:
  PtrType data_;
  const int64_t* sizes_;
  const int64_t* strides_;
};

// The `TensorAccessor` is typically instantiated for CPU `Tensor`s using
// `Tensor.accessor<T, N>()`.
// For CUDA `Tensor`s, `PackedTensorAccessor` is used on the host and only
// indexing on the device uses `TensorAccessor`s.
template<typename T, size_t N, template <typename U> class PtrTraits = DefaultPtrTraits>
class TensorAccessor : public TensorAccessorBase<T,N,PtrTraits> {
public:
  typedef typename PtrTraits<T>::PtrType PtrType;

  AT_HOSTDEVICE TensorAccessor(PtrType data_, const int64_t * sizes_, const int64_t * strides_)
  : TensorAccessorBase<T,N>(data_,sizes_,strides_) {}

  AT_HOSTDEVICE TensorAccessor<T,N-1> operator[](int64_t i) {
    return TensorAccessor<T,N-1>(this->data_ + this->strides_[0]*i,this->sizes_+1,this->strides_+1);
  }

  AT_HOSTDEVICE const TensorAccessor<T,N-1> operator[](int64_t i) const {
    return TensorAccessor<T,N-1>(this->data_ + this->strides_[0]*i,this->sizes_+1,this->strides_+1);
  }
};

template<typename T, template <typename U> class PtrTraits>
class TensorAccessor<T,1,PtrTraits> : public TensorAccessorBase<T,1,PtrTraits> {
public:
  typedef typename PtrTraits<T>::PtrType PtrType;

  AT_HOSTDEVICE TensorAccessor(PtrType data_, const int64_t * sizes_, const   int64_t * strides_)
  : TensorAccessorBase<T,1,PtrTraits>(data_,sizes_,strides_) {}
  AT_HOSTDEVICE T & operator[](int64_t i) {
    return this->data_[this->strides_[0]*i];
  }
};


// PackedTensorAccessorBase and PackedTensorAccessor are used on for CUDA `Tensor`s on the host
// and as
// In contrast to `TensorAccessor`s, they copy the strides and sizes on instantiation (on the host)
// in order to transfer them on the device when calling kernels.
// On the device, indexing of multidimensional tensors gives to `TensorAccessor`s.
// Use RestrictPtrTraits as PtrTraits if you want the tensor's data pointer to be marked as __restrict__.
// Instantiation from data, sizes, strides is only needed on the host and std::copy isn't available
// on the device, so those functions are host only.
template<typename T, size_t N, template <typename U> class PtrTraits = DefaultPtrTraits>
class PackedTensorAccessorBase {
public:
  typedef typename PtrTraits<T>::PtrType PtrType;
  AT_HOST PackedTensorAccessorBase(PtrType data_, const int64_t * sizes_, const   int64_t * strides_)
  : data_(data_)
  {
    std::copy(sizes_, sizes_ + N, std::begin(this->sizes_));
    std::copy(strides_, strides_ + N, std::begin(this->strides_));
  }
  AT_HOSTDEVICE int64_t stride(int64_t i) const { return strides_[i]; }
  AT_HOSTDEVICE int64_t size(int64_t i) const { return sizes_[i]; }
protected:
  PtrType data_;
  int64_t sizes_[N];
  int64_t strides_[N];
};

template<typename T, size_t N, template <typename U> class PtrTraits = DefaultPtrTraits>
class PackedTensorAccessor : public PackedTensorAccessorBase<T,N,PtrTraits> {
public:
  typedef typename PtrTraits<T>::PtrType PtrType;

  AT_HOST PackedTensorAccessor(PtrType data_, const int64_t * sizes_, const   int64_t * strides_)
  : PackedTensorAccessorBase<T,N,PtrTraits>(data_, sizes_, strides_) {};

  AT_DEVICE TensorAccessor<T,N-1> operator[](int64_t i) {
    int64_t* new_sizes = this->sizes_+1;
    int64_t* new_strides = this->strides_+1;
    return TensorAccessor<T,N-1>(this->data_ + this->strides_[0]*i, new_sizes, new_strides);
  }

  AT_DEVICE const TensorAccessor<T,N-1> operator[](int64_t i) const {
    int64_t* new_sizes = this->sizes_+1;
    int64_t* new_strides = this->strides_+1;
    return TensorAccessor<T,N-1>(this->data_ + this->strides_[0]*i, new_sizes, new_strides);
  }
};

template<typename T, template <typename U> class PtrTraits>
class PackedTensorAccessor<T,1,PtrTraits> : public PackedTensorAccessorBase<T,1,PtrTraits> {
public:
  typedef typename PtrTraits<T>::PtrType PtrType;
  AT_HOST PackedTensorAccessor(PtrType data_, const int64_t * sizes_, const   int64_t * strides_)
  : PackedTensorAccessorBase<T,1,PtrTraits>(data_, sizes_, strides_) {};

  AT_DEVICE T & operator[](int64_t i) {
    return this->data_[this->strides_[0]*i];
  }
  AT_DEVICE const T& operator[](int64_t i) const {
    return this->data_[this->strides_[0]*i];
  }
};

}

#undef AT_HOSTDEVICE
#undef AT_HOST
#undef AT_DEVICE
