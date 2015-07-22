#include "EncVector.hpp"
#include "EncMatrix.hpp"
#include "fhe/replicate.h"
#include <NTL/ZZX.h>
namespace MDL {
EncVector& EncVector::pack(const Vector<long>  & vec,
                           const EncryptedArray& ea)
{
    assert(vec.size() <= ea.size());

    if (vec.size() < ea.size()) {
        auto tmp(vec);
        tmp.resize(ea.size(), 0);
        ea.encrypt(*this, getPubKey(), tmp);
    } else {
        ea.encrypt(*this, getPubKey(), vec);
    }
    return *this;
}

std::vector<EncVector>EncVector::partition_pack(const Vector<long>  & vec,
                                                const FHEPubKey     & pk,
                                                const EncryptedArray& ea)
{
    auto parts_nr = (vec.size() + ea.size() - 1) / ea.size();
    std::vector<EncVector> ctxts(parts_nr, pk);

    return ctxts;
}

template<>
bool EncVector::unpack(Vector<long>        & result,
                       const FHESecKey     & sk,
                       const EncryptedArray& ea) const
{
    ea.decrypt(*this, sk, result);
    return this->isCorrect();
}

template<>
bool EncVector::unpack(Vector<NTL::ZZX>    & result,
                       const FHESecKey     & sk,
                       const EncryptedArray& ea) const
{
    ea.decrypt(*this, sk, result);
    return this->isCorrect();
}

EncVector& EncVector::dot(const EncVector     & oth,
                          const EncryptedArray& ea)
{
    this->multiplyBy(oth);
    totalSums(ea, *this);
    return *this;
}

EncMatrix EncVector::covariance(const EncryptedArray& ea, long actualDimension)
{
    EncMatrix mat(getPubKey());
    actualDimension = actualDimension == 0 ? ea.size() : actualDimension;
    for (size_t i = 0; i < actualDimension; i++) {
        mat.push_back(*this);
        auto tmp(*this);
        replicate(ea, tmp, i);
        mat[i].multiplyBy(tmp);
    }
    return mat;
}
} // namespace MDL
