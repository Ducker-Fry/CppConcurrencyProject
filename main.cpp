#include<iostream>
#include<matrix/matrix.h>

using namespace matrix;

int main() 
{
    Matrix<int> m1(3, 3);
    Matrix<int> m2(3, 3);
    Matrix<int> m3(3, 3);
    m1.fill(1);
    m2.fill(2);
    m3.fill(3);

    // addition
    auto m4 = m1 + m2;
    for (int i = 0; i < m4.rows(); i++)
    {
        for (int j = 0; j < m4.cols(); j++)
        {
            std::cout << m4(i, j) << " ";
        }
        std::cout << std::endl;
    }

    return 0;
}
