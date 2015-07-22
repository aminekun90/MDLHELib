#include "utils/MatrixAlgebraUtils.hpp"
#include "utils/FHEUtils.hpp"
#include "fhe/replicate.h"
#include "EncMatrix.hpp"
#include <thread>
namespace MDL {
EncMatrix& EncMatrix::pack(const Matrix<long>  & mat,
                           const EncryptedArray& ea)
{
    this->resize(mat.rows(), _pk);

    for (size_t r = 0; r < mat.rows(); r++) {
        this->at(r).pack(mat[r], ea);
    }
    return *this;
}

template<>
void EncMatrix::unpack(Matrix<long>        & result,
                       const FHESecKey     & sk,
                       const EncryptedArray& ea) const
{
    result.resize(this->size());

    for (size_t r = 0; r < this->size(); r++) {
        this->at(r).unpack(result[r], sk, ea);
    }
}

template<>
void EncMatrix::unpack(Matrix<NTL::ZZX>    & result,
                       const FHESecKey     & sk,
                       const EncryptedArray& ea) const
{
    result.resize(this->size());

    for (size_t r = 0; r < this->size(); r++) {
        this->at(r).unpack(result[r], sk, ea);
    }
}

EncVector EncMatrix::dot(const EncVector     & oth,
                         const EncryptedArray& ea) const
{
    std::vector<EncVector> result(this->size(),
                                  oth.getPubKey());
    std::vector<std::thread> workers;
    std::atomic<size_t> counter(0);

    for (int wr = 0; wr < WORKER_NR; wr++) {
        workers.push_back(std::move(std::thread([this, &result,
                                                 &counter, &oth, &ea]()
            {
                size_t next;

                while ((next = counter.fetch_add(1)) < ea.size()) {
                    result[next] = this->at(next);
                    result[next].dot(oth, ea);
                    auto one_bit_mask = make_bit_mask(ea, next);
                    result[next].multByConstant(one_bit_mask);
                }
            })));
    }

    for (auto && wr : workers) wr.join();

    for (size_t r = 1; r < result.size(); r++) {
        result[0] += result[r];
    }
    return result[0];
}

EncVector EncMatrix::column_dot(const EncVector     & oth,
                                const EncryptedArray& ea) const
{
    std::vector<EncVector>   parts(this->size(), this->at(0).getPubKey());
    std::atomic<size_t>      counter(0);
    std::vector<std::thread> workers;

    for (int wr = 0; wr < WORKER_NR; wr++) {
        workers.push_back(std::move(std::thread([&oth, &ea, &parts,
                                                 &counter, this]() {
                size_t c;

                while ((c = counter.fetch_add(1)) < ea.size()) {
                    EncVector vec(oth);
                    replicate(ea, vec, c);
                    vec.multiplyBy(this->at(c));
                    parts[c] = vec;
                }
            })));
    }

    for (auto && wr : workers) {
        wr.join();
    }

    EncVector result(parts[0]);

    for (size_t i = 1; i < this->size(); i++) {
        result += parts[i];
    }
    return result;
}

EncMatrix& EncMatrix::transpose(const EncryptedArray& ea)
{
    // need to be square matrix
    if (ea.size() != this->size()) return *this;

    auto dim = ea.size();
    auto mat = *this;

    for (size_t row = 1; row < dim; row++) {
        ea.rotate(mat[row], row);
    }

    for (size_t col = 0; col < dim; col++) {
        auto new_col = mat[0];
        new_col.multByConstant(make_bit_mask(ea, col));

        for (size_t row = 1; row < dim; row++) {
            auto tmp(mat[row]);
            tmp.multByConstant(make_bit_mask(ea, (col + row) % dim));
            new_col += tmp;
        }

        ea.rotate(new_col, -col);
        this->at(col) = new_col;
    }
    return *this;
}

EncMatrix& EncMatrix::operator+=(const EncMatrix& oth)
{
    if (this->size() != oth.size()) {
        fprintf(stderr, "Warnning! adding two mismatch size matrix!\n");
        return *this;
    }

    for (size_t i = 0; i < this->size(); i++) this->at(i) += oth[i];
    return *this;
}

EncMatrix& EncMatrix::operator-=(const EncMatrix& oth)
{
    if (this->size() != oth.size()) {
        fprintf(stderr, "Warnning! adding two mismatch size matrix!\n");
        return *this;
    }

    for (size_t i = 0; i < this->size(); i++) this->at(i) -= oth[i];
    return *this;
}

EncMatrix& EncMatrix::multByConstant(const NTL::ZZX cons)
{
    for (size_t i = 0; i < this->size(); i++) this->at(i).multByConstant(cons);
    return *this;
}
} // namespace MDL
