#pragma once

#include <stdint.h>

namespace sniffer {

enum class FilterResult : uint8_t {
  Ok,
  InvalidId,  // ID acima de 0x7FF
  TooMany,    // excede a capacidade da lista
};

// Lista de IDs a exibir. Lista vazia = filtro desativado (exibe tudo).
// Todas as operações que falham deixam o estado inalterado.
template <uint8_t N>
class IdFilter {
 public:
  static const uint16_t kMaxId = 0x7FF;

  IdFilter() : count_(0) {}

  bool active() const { return count_ > 0; }
  uint8_t count() const { return count_; }
  uint16_t at(uint8_t i) const { return ids_[i]; }

  bool allows(uint16_t id) const { return count_ == 0 || contains(id); }

  void clear() { count_ = 0; }

  // Adiciona um ID (idempotente). Com o filtro desativado, ativa-o só com esse ID.
  FilterResult add(uint16_t id) {
    if (id > kMaxId) return FilterResult::InvalidId;
    if (contains(id)) return FilterResult::Ok;
    if (count_ == N) return FilterResult::TooMany;
    ids_[count_++] = id;
    return FilterResult::Ok;
  }

  // Remove um ID (idempotente). Remover o último desativa o filtro.
  FilterResult remove(uint16_t id) {
    if (id > kMaxId) return FilterResult::InvalidId;
    for (uint8_t i = 0; i < count_; i++) {
      if (ids_[i] == id) {
        for (uint8_t j = i; j + 1 < count_; j++) ids_[j] = ids_[j + 1];
        --count_;
        break;
      }
    }
    return FilterResult::Ok;
  }

  // Substitui a lista. n > N é rejeitado mesmo que haja repetidos.
  FilterResult set(const uint16_t* ids, uint8_t n) {
    if (n > N) return FilterResult::TooMany;
    IdFilter next;
    for (uint8_t i = 0; i < n; i++) {
      FilterResult r = next.add(ids[i]);
      if (r != FilterResult::Ok) return r;
    }
    *this = next;
    return FilterResult::Ok;
  }

 private:
  bool contains(uint16_t id) const {
    for (uint8_t i = 0; i < count_; i++) {
      if (ids_[i] == id) return true;
    }
    return false;
  }

  uint16_t ids_[N];
  uint8_t count_;
};

}  // namespace sniffer
