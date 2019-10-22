#include <iostream>
#include <omp.h>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_set>
#include <algorithm>
#include <regex>

using namespace std;

u_int64_t revComp(u_int64_t x, size_t sizeKmer=15)
{
    u_int64_t res = x;

    res = ((res>> 2 & 0x3333333333333333) | (res & 0x3333333333333333) <<  2);
    res = ((res>> 4 & 0x0F0F0F0F0F0F0F0F) | (res & 0x0F0F0F0F0F0F0F0F) <<  4);
    res = ((res>> 8 & 0x00FF00FF00FF00FF) | (res & 0x00FF00FF00FF00FF) <<  8);
    res = ((res>>16 & 0x0000FFFF0000FFFF) | (res & 0x0000FFFF0000FFFF) << 16);
    res = ((res>>32 & 0x00000000FFFFFFFF) | (res & 0x00000000FFFFFFFF) << 32);
    res = res ^ 0xAAAAAAAAAAAAAAAA;

    return (res >> (2*( 32 - sizeKmer))) ;
}

u_int64_t toDeci(string &acgt, size_t start, size_t size)
{
    u_int64_t val = 0;

    for (size_t i = start; i < start + size; i++)
    {
        val = (val << 2);
        val += (acgt[i]>>1&3);
        
    }

    return val;
}

vector<long> splitLine(string &line)
{
    vector<long> vec(2);
    string tmp = "";

    for (int i = 0; i < (int)line.length(); i++)
    {
        if (line[i] == ',' || line[i] == ' ')
        {
            vec[0] = stol(tmp);
            tmp = "";
            continue;
        }
        else
        {
            tmp += line[i];
        }
    }

    vec[1] = stol(tmp);

    return vec;
}

void readKmerFile(string filename, vector<long> &kmers)
{
    ifstream myfile(filename);
    string line;
    vector<long> v;
    while (getline(myfile, line))
    {
        v = splitLine(line);
        kmers[v[0]] = v[1];
        kmers[revComp(v[0])] = v[1];
    }

    myfile.close();

    return;
}

double *processLine(string &line, vector<long> &allKmers)
{
    double *counts = new double[32];
    long sum = 0, count, pos, k_size = 15, bin_size=5;
    u_int64_t kmer;
    static regex validKmers("^[CAGT]+$");

    // to avoid garbage memory
    for (int i = 0; i < 32; i++)
    {
        counts[i] = 0;
    }

    for (size_t i = 0; i < line.length() - k_size - 1; i++)
    {
        //ignore kmers with non ACGT characters
        if (!regex_match(line.substr(i, k_size),  validKmers)) {
            continue;
        }

        kmer = toDeci(line, i, k_size);
        count = allKmers[(long)kmer];
        pos = (count / bin_size) - 1;

        if (count <= bin_size)
        {
            counts[0]++;
            
        }
        else if (pos < 32 && pos > 0)
        {
            counts[pos]++;
        }
        sum++;
    }

    for (int i = 0; i < 32; i++)
    {
        counts[i] /= sum;
        if (counts[i] <  1e-4)
        {
            counts[i] = 0;
        }
    }

    return counts;
}

void processLinesBatch(vector<string> &linesBatch, vector<long> &allKmers, string &outputPath, int threads)
{
    vector<double *> batchAnswers(linesBatch.size());

    #pragma omp parallel for num_threads(threads)
    for (uint i = 0; i < linesBatch.size(); i++)
    {
        batchAnswers[i] = processLine(linesBatch[i], allKmers);
    }

    ofstream output;
    output.open(outputPath, ios::out | ios::app);
    string results = "";

    for (uint i = 0; i < linesBatch.size(); i++)
    {
        for (int j = 0; j < 32; j++)
        {
            results += to_string(batchAnswers[i][j]);

            if (j < 32 - 1)
            {
                results += " ";
            }
        }

        results += "\n";

        // releasing pointer memory
        delete[] batchAnswers[i];
    }

    output << results;
    batchAnswers.clear();
    output.close();
}

int main(int argc, char ** argv)
{
    vector<long> kmers(1073741824, 0);
    vector<string> batch;
    long lineNum = 0;

    string kmersFile =  argv[1];
    cout << "K-Mer file " << kmersFile << endl;

    cout << "LOADING KMERS TO RAM" << endl;
    readKmerFile(kmersFile, kmers);

    cout << "FINISHED LOADING KMERS TO RAM " << kmers.size() << endl;

    string inputPath =  argv[2];
    string outputPath =  argv[3];
    int threads = 8;

    if (argv[4] != NULL) 
        threads = stoi(argv[4]);

    cout << "INPUT FILE " << inputPath << endl;
    cout << "OUTPUT FILE " << outputPath << endl;
    cout << "THREADS " << threads << endl;

    ifstream myfile(inputPath);
    string line;

    ofstream output;
    output.open(outputPath, ios::out);
    output.close();

    while (getline(myfile, line))
    {
        if (lineNum % 4 != 1)
        {
            lineNum++;
            continue;
        }
        else
        {
            batch.push_back(line);
        }

        lineNum++;

        if (batch.size() == 10000)
        {
            processLinesBatch(batch, kmers, outputPath, threads);
            batch.clear();
        }
    }
    processLinesBatch(batch, kmers, outputPath, threads);

    myfile.close();
    batch.clear();

    kmers.clear();

    cout << "COMPLETED : Output at - " << outputPath << endl;

    return 0;
}