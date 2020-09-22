import numpy
import sys
import random
import scipy.sparse

class TestA:
    sample_c = """
    #include "libthpool.h"
    #include <stdio.h>
    #include <unistd.h>
    #include <stdlib.h>
    #include <string.h>
    #include <sys/time.h>

    int main()
    {{
        TaskMatrixInfoA* tasks[{0}];
        {1}
        struct timeval stop, start;
        gettimeofday(&start, NULL);
        gettimeofday(&start, NULL);
        matrix_calc_taskA(tasks, {0});
        gettimeofday(&stop, NULL);
        {2}
        {3}
        printf("Program Running Time on Task A: %f ms\\n", (stop.tv_sec - start.tv_sec) * 1000.0f + (stop.tv_usec - start.tv_usec) / 1000.0f);
        return 0;
    }}
    """

    init_structA = """
        TaskMatrixInfoA* task_{0} = malloc(sizeof(TaskMatrixInfoA));
        int rowArray_{0}[] = {1};
        task_{0}->rowArray = rowArray_{0};
        int rowOffset_{0}[] = {2};
        task_{0}->rowOffset = rowOffset_{0};
        task_{0}->rowArraySize = {3};
        int columnIndice_{0}[] = {4};
        task_{0}->columnIndice = columnIndice_{0};
        double S_{0}[] = {5};
        task_{0}->S = S_{0};
        double valueNormalMatrix_{0}[] = {6};
        task_{0}->valueNormalMatrix = valueNormalMatrix_{0};
        task_{0}->Id = malloc(sizeof(double) * {7});
        double Real_Id_{0}[{7}] = {8};
        tasks[{0}] = task_{0};
    """

    free_structA = """
        free(task_{0}->Id);
        free(task_{0});
    """

    check_structA = """
        int flag_{1} = 0;
        for (int i = 0; i < {0}; ++i)
        {{
            if (Real_Id_{1}[i] - task_{1}->Id[i] > 1e-6)
            {{
                flag_{1} = 1;
                break;
            }}
        }}
        if (flag_{1})
        {{
            for (int i = 0; i < {0}; ++i)
                printf("%f %f\\n", Real_Id_{1}[i], task_{1}->Id[i]);
            printf("Check Failed For Test {1}\\n");
        }}
        /*
        else
            printf("Check Success For Test {1}\\n");
        */
    """

    def ArrayWrapper(self, a):
        return "{" + ','.join(map(str, a)) + "}"

    def GenerateTestA(self, t, n, density, c):
        init_c = ""
        free_c = ""
        check_c = ""

        for i in range(t):
            # Generate A(n*n)
            matrix = scipy.sparse.rand(n,n,density,'csr',float)
            A = scipy.sparse.csr_matrix(matrix)
            # Generate S(n*1)
            S = numpy.random.rand(n)
            # Generate Target Rows to Compute
            C = numpy.sort(random.sample(range(n), c))
            # Full Result without Mask
            Id = numpy.matmul(matrix.toarray(), numpy.transpose(S))
            # Apply Mask
            for j in range(n):
                if j in C:
                    continue
                Id[j] = 0
            
        
        
            init_c += self.init_structA.format(
                i, self.ArrayWrapper(C), self.ArrayWrapper(A.indptr), len(C), self.ArrayWrapper(A.indices), self.ArrayWrapper(S), self.ArrayWrapper(A.data), n, self.ArrayWrapper(Id))

            free_c += self.free_structA.format(i)
            check_c += self.check_structA.format(n, i)
        file_c = self.sample_c.format(t, init_c, check_c, free_c)
        with open("test.c", "w+") as file: #t, n, str(density).replace(".", "-"), c
            file.writelines(file_c)
        return

if __name__ == '__main__':
    testA = TestA()
    testA.GenerateTestA(int(sys.argv[1]), int(sys.argv[2]), float(sys.argv[3]), int(sys.argv[4]))