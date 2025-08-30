#pragma once
#include <vector>
#include <cassert>
#include <cstdlib>
#include <stdexcept>
#include <memory>
#include <type_traits>

namespace matrix
{

    template<typename T, size_t Alignment = 64>
    struct MatrixAllocator
    {
        using value_type = T;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;
        using size_type = size_t;
        using difference_type = std::ptrdiff_t;

        template<typename U>
        struct rebind
        {
            using other = MatrixAllocator<U, Alignment>;
        };

        MatrixAllocator() = default;

        template<typename U>
        MatrixAllocator(const MatrixAllocator<U>&) noexcept {}

        pointer allocate(size_t n)
        {
            if (n > max_size())
            {
                throw std::bad_alloc();
            }

#if defined(_MSC_VER)
            pointer ptr = static_cast<pointer>(
                _aligned_malloc(n * sizeof(T), Alignment)
                );
            if (!ptr)
            {
                throw std::bad_alloc();
            }
#else
            pointer ptr = static_cast<pointer>(
                std::aligned_alloc(Alignment, n * sizeof(T))
                );
            if (!ptr)
            {
                throw std::bad_alloc();
            }
#endif

            return ptr;
        }

        void deallocate(pointer ptr, size_t) noexcept
        {
#if defined(_MSC_VER)
            _aligned_free(ptr);
#else
            std::free(ptr);
#endif
        }

        size_type max_size() const noexcept
        {
            return std::numeric_limits<size_type>::max() / sizeof(T);
        }

        template<typename U, typename... Args>
        void construct(U* p, Args&&... args)
        {
            ::new(static_cast<void*>(p)) U(std::forward<Args>(args)...);
        }

        template<typename U>
        void destroy(U* p)
        {
            p->~U();
        }
    };

    template<typename T, size_t Alignment>
    bool operator==(const MatrixAllocator<T, Alignment>&,
        const MatrixAllocator<T, Alignment>&) noexcept
    {
        return true;
    }

    template<typename T, size_t Alignment, typename U>
    bool operator==(const MatrixAllocator<T, Alignment>&,
        const MatrixAllocator<U, Alignment>&) noexcept
    {
        return false;
    }

    template<typename T>
    class Matrix
    {
    private:
        using AlignedVector = std::vector<T, MatrixAllocator<T>>;
        size_t rows_;
        size_t cols_;
        size_t leading_dim_;
        AlignedVector data_;
        bool row_major_;

        // Helper function for index calculation
        size_t calculate_index(size_t row, size_t col) const
        {
            assert(row < rows_ && col < cols_ && "Index out of bounds");
            return row_major_ ? row * cols_ + col : col * rows_ + row;
        }

    public:
        // Default constructor
        Matrix() : rows_(0), cols_(0), leading_dim_(0), row_major_(true) {}

        // Constructor with dimensions
        Matrix(size_t rows, size_t cols, bool row_major = true)
            : rows_(rows), cols_(cols),
            leading_dim_(row_major ? rows : cols),
            row_major_(row_major)
        {
            data_.resize(rows * cols, T());
        }

        // Copy constructor
        Matrix(const Matrix& other) = default;

        // Move constructor
        Matrix(Matrix&& other) noexcept = default;

        // Copy assignment
        Matrix& operator=(const Matrix& other) = default;

        // Move assignment
        Matrix& operator=(Matrix&& other) noexcept = default;

        // Destructor
        ~Matrix() = default;

        // Element access
        T& operator()(size_t row, size_t col)
        {
            return data_[calculate_index(row, col)];
        }

        const T& operator()(size_t row, size_t col) const
        {
            return data_[calculate_index(row, col)];
        }

        // Dimension accessors
        size_t rows() const { return rows_; }
        size_t cols() const { return cols_; }
        size_t leading_dim() const { return leading_dim_; }
        T* data() { return data_.data(); }
        const T* data() const { return data_.data(); }
        bool row_major() const { return row_major_; }

        // Fill the matrix with a constant value
        void fill(const T& value)
        {
            std::fill(data_.begin(), data_.end(), value);
        }
    };

    // Overload the << operator for Matrix
    // overload the add operator for Matrix
    template<typename T>
    Matrix<T> operator+(const Matrix<T>& a, const Matrix<T>& b)
    {
        assert(a.rows() == b.rows() && a.cols() == b.cols() && "Matrix dimensions do not match");
        Matrix<T> result(a.rows(), a.cols());
        for (size_t i = 0; i < a.rows(); ++i)
        {
            for (size_t j = 0; j < a.cols(); ++j)
            {
                result(i, j) = a(i, j) + b(i, j);
            }
        }

        return result;
    }

    // overload the subtract operator for Matrix
    template<typename T>
    Matrix<T> operator-(const Matrix<T>& a, const Matrix<T>& b)
    {
        assert(a.rows() == b.rows() && a.cols() == b.cols() && "Matrix dimensions do not match");
        Matrix<T> result(a.rows(), a.cols());
        for (size_t i = 0; i < a.rows(); ++i)
        {
            for (size_t j = 0; j < a.cols(); ++j)
            {
                result(i, j) = a(i, j) - b(i, j);
            }
        }

        return result;
    }

    // overload the multiply operator for Matrix
    template<typename T>
    Matrix<T> operator*(const Matrix<T>& a, const Matrix<T>& b)
    {
        assert(a.cols() == b.rows() && "Matrix dimensions do not match");
        Matrix<T> result(a.rows(), b.cols());
        for (size_t i = 0; i < a.rows(); ++i)
        {
            for (size_t j = 0; j < b.cols(); ++j)
            {
                for (size_t k = 0; k < a.cols(); ++k)
                {
                    result(i, j) += a(i, k) * b(k, j);
                }
            }
        }

        return result;
    }

    // overload the matrix and constant addition operator
    template<typename T>
    Matrix<T> operator+(const Matrix<T>& a, const T& b)
    {
        Matrix<T> result(a.rows(), a.cols());
        for (size_t i = 0; i < a.rows(); ++i)
        {
            for (size_t j = 0; j < a.cols(); ++j)
            {
                result(i, j) = a(i, j) + b;
            }
        }

        return result;
    }

}
