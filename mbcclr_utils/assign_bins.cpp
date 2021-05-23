#include <omp.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>

// TODO use valarrays for faster computations
using namespace std;

class Bin
{
private:
    vector<long double> p3mean, p3std, p15mean, p15std;
    string name;
public:
    Bin()
    {
        this->name = "UnBinned";
    }

    Bin(string name)
    {
        this->name = name;
    }

    Bin(string name, vector<long double> p3mean, vector<long double> p15mean,  vector<long double> p3std,  vector<long double> p15std)
    {
        this->name = name;
        this->p3mean = p3mean;
        this->p3std = p3std;
        this->p15mean = p15mean;
        this->p15std = p15std;
    }

    string getName()
    {
        return this->name;
    }

    long double logpdf(long double mu, long double s, long double x)
    {
        long double p = -0.5L * powl((x - mu)/s, 2.0L) - log(powl(2.0L * 22.0L / 7.0L, 0.5L) * s);
        
        return p;
    }


    long double getProbability15(vector<long double> p15)
    {
        size_t size = p15.size();
        long double prob = 0;        

        for (size_t i = 0; i < size; i++)
        {
            prob += logpdf(this->p15mean[i], this->p15std[i], p15[i]);
        }
        
        return prob;
    }

    long double getProbability3(vector<long double> p3)
    {
        size_t size = p3.size();
        long double prob = 0;        

        for (size_t i = 0; i < size; i++)
        {
            prob += logpdf(this->p3mean[i], this->p3std[i], p3[i]);
        }
        
        return prob;
    }

    void setP3mean(vector<long double> vals)
    {
        this->p3mean = vals;
    }

    void setP3std(vector<long double> vals)
    {
        this->p3std = vals;
    }

    void setP15mean(vector<long double> vals)
    {
        this->p15mean = vals;
    }

    void setP15std(vector<long double> vals)
    {
        this->p15std = vals;
    }
};

vector<long double> lineToVec(string &line)
{
    vector<long double> values;
    string tmp = "";

    for (int i = 0; i < (int)line.length(); i++)
    {
        if ((48 <= int(line[i]) && int(line[i]) <= 57) || line[i] == '.' || line[i] == '+' || line[i] == '-' || line[i] == 'e')
        {
            tmp += line[i];
        }
        else if (tmp.length() > 0)
        {

            values.push_back(stold(tmp));
            tmp = "";
        }
    }
    if (tmp.length() > 0)
    {
        values.push_back(stold(tmp));
        tmp = "";
    }

    return values;
}

vector<Bin> getBinsFromFile(string &statsFile)
{
    ifstream infile(statsFile);
    string line;
    vector<Bin> bins;

    while (getline(infile, line))
    {
        Bin bin(line);
        
        getline(infile, line);
        bin.setP15mean(lineToVec(line));
        
        getline(infile, line);
        bin.setP3mean(lineToVec(line));
        
        getline(infile, line);
        bin.setP15std(lineToVec(line));
        
        getline(infile, line);
        bin.setP3std(lineToVec(line));

        bins.push_back(bin);
    }

    infile.close();

    return bins;
}

Bin getBestBin(vector<Bin> &bins, vector<long double> &p3, vector<long double> &p15)
{
    Bin bestbin;
    long double maxProb15 = -INFINITY, maxProb3 = -INFINITY;
    long double prob15, prob3;

    for (Bin bin: bins)
    {
        prob15 = bin.getProbability15(p15);
        prob3 = bin.getProbability3(p3);

        if (prob15 > maxProb15)
        {
            maxProb15 = prob15;
        }

        if (prob15 == maxProb15 && prob3 > maxProb3)
        {
            bestbin = bin;
            maxProb3 = prob3;
        }
    }

    return bestbin;
}

void processLinesBatch(vector<vector<long double>> &batch3, vector<vector<long double>> &batch15, vector<Bin> &bins, string &outputPath, int threads)
{
    vector<string> batchAnswers(batch3.size());
    string result = "";

    #pragma omp parallel for num_threads(threads) schedule(static, threads)
    for (size_t i = 0; i < batchAnswers.size(); i++)
    {
        
        batchAnswers[i] = getBestBin(bins, batch3[i], batch15[i]).getName();
    }

    ofstream output;
    output.open(outputPath, ios::out | ios::app);

    for (size_t i = 0; i < batchAnswers.size(); i++)
    {
        result += batchAnswers[i];
        result += "\n";
    }

    output << result;

    batchAnswers.clear();
    output.close();
}

int main(int argc, char ** argv)
{
    string p3 = argv[1];
    string p15 = argv[2];
    string statsFile = argv[3];
    int threads = stoi(argv[4]);
    string outputPath = argv[5];
    vector<vector<long double>> batch3;
    vector<vector<long double>> batch15;

    vector<Bin> bins = getBinsFromFile(statsFile);

    cout << "3 Mers " <<  p3 << endl;
    cout << "15 Mers " <<  p15 << endl;
    cout << "Stats " <<  statsFile << endl;
    cout << "Threads " <<  threads << endl;

    cout << "Bins size = " << bins.size() << endl;


    ifstream p3File(p3);
    ifstream p15File(p15);
    string line;

    ofstream output;
    output.open(outputPath, ios::out);
    output.close();

    while (getline(p3File, line))
    {
        batch3.push_back(lineToVec(line));
        getline(p15File, line);
        batch15.push_back(lineToVec(line));
        
        if (batch3.size() == 1000000)
        {
            processLinesBatch(batch3, batch15, bins, outputPath, threads);
            batch3.clear();
            batch15.clear();
        }
    }
    processLinesBatch(batch3, batch15, bins, outputPath, threads);

    batch3.clear();
    batch15.clear();

}