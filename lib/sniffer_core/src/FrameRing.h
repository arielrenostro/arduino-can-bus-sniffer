#pragma once

#include <stdint.h>

#include "Frame.h"

namespace sniffer {

// Ring buffer FIFO de quadros. Quando cheio, o quadro novo é descartado e
// lost() é incrementado. N deve ser potência de 2 (indexação por máscara).
template <uint8_t N>
class FrameRing {
  static_assert(N >= 2 && N <= 128 && (N & (N - 1)) == 0,
                "N deve ser potencia de 2 entre 2 e 128");

 public:
  FrameRing() : head_(0), tail_(0), lost_(0) {}

  uint8_t size() const { return (uint8_t)(head_ - tail_); }
  bool empty() const { return size() == 0; }
  bool full() const { return size() == N; }
  uint32_t lost() const { return lost_; }

  // Retorna false (e conta uma perda) se o ring estiver cheio.
  bool push(const Frame& f) {
    if (full()) {
      ++lost_;
      return false;
    }
    buf_[head_ & (N - 1)] = f;
    ++head_;
    return true;
  }

  // Quadro mais antigo sem removê-lo; nullptr se vazio.
  const Frame* peek() const { return empty() ? nullptr : &buf_[tail_ & (N - 1)]; }

  // Remove o quadro mais antigo para `out`; false se vazio.
  bool pop(Frame& out) {
    if (empty()) return false;
    out = buf_[tail_ & (N - 1)];
    ++tail_;
    return true;
  }

  // Descarta o quadro mais antigo (depois de peek()).
  void drop() {
    if (!empty()) ++tail_;
  }

 private:
  Frame buf_[N];
  uint8_t head_;  // incrementa e dá a volta em 256; N divide 256
  uint8_t tail_;
  uint32_t lost_;
};

}  // namespace sniffer
