#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <curl/curl.h>
#include <math.h>
#include <algorithm>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

using namespace boost::property_tree;

void PRINTM(std::vector<std::vector<double>> x){
    for(auto & i : x){
        for(auto & j : i){
            std::cout << j << "\t";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

std::vector<std::vector<double>> MMULT(std::vector<std::vector<double>> x, std::vector<std::vector<double>> y){
    std::vector<std::vector<double>> z;
    std::vector<double> temp;
    double total = 0;

    for(int i = 0; i < x.size(); ++i){
        temp.clear();
        for(int j = 0; j < y[0].size(); ++j){
            total = 0;
            for(int k = 0; k < x[0].size(); ++k){
                total += x[i][k]*y[k][j];
            }
            temp.push_back(total);
        }
        z.push_back(temp);
    }

    return z;
}

std::vector<std::vector<double>> TRANSPOSE(std::vector<std::vector<double>> x){
    std::vector<std::vector<double>> z;
    std::vector<double> temp;
    for(int i = 0; i < x[0].size(); ++i){
        temp.clear();
        for(int j = 0; j < x.size(); ++j){
            temp.push_back(x[j][i]);
        }
        z.push_back(temp);
    }
    return z;
}

std::vector<std::vector<double>> Factor(double a, std::vector<std::vector<double>> z){
    for(int i = 0; i < z.size(); ++i){
        for(int j = 0; j < z[0].size(); ++j){
            z[i][j] *= a;
        }
    }
    return z;
}

// Callback function to handle the response data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string GET(std::string ticker) {
    std::string url = "https://api.exchange.coinbase.com/products/" + ticker + "/candles?granularity=60";
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if (curl) {
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "User-Agent: MacOS");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }
        curl_easy_cleanup(curl);
    }

    return readBuffer;
}

ptree JSON(std::string msg){
    std::stringstream ss(msg);
    ptree result;
    read_json(ss, result);
    return result;
}

void CYCLONE(ptree dataset, std::string ticker, std::map<std::string, std::vector<double>> & Z){
    for(ptree::const_iterator it = dataset.begin(); it != dataset.end(); ++it){
        int count = 0;
        for(ptree::const_iterator jt = it->second.begin(); jt != it->second.end(); ++jt){
            if(count == 4){
                Z[ticker].push_back(jt->second.get_value<double>());
            }
            count += 1;
        }
    }
}

int main()
{
    std::map<std::string, std::vector<double>> df;
    std::vector<std::string> tickers = {"BTC-USD","ETH-USD","ADA-USD","SOL-USD"};

    for(auto & tick : tickers){
        CYCLONE(JSON(GET(tick)), tick, std::ref(df));
    }

    std::vector<std::vector<double>> close, ror;
    for(auto & tick : tickers){
        close.push_back(df[tick]);
    }

    close = TRANSPOSE(close);

    std::vector<double> hold;
    for(int i = 1; i < close.size(); ++i){
        hold.clear();
        for(int j = 0; j < close[i].size(); ++j){
            hold.push_back(close[i-1][j]/close[i][j] - 1.0);
        }
        ror.push_back(hold);
    }

    int M = ror.size();
    int N = ror[0].size();

    std::vector<std::vector<double>> ones, mu;
    for(int i = 0; i < M; ++i){
        ones.push_back({1.0});
    }

    mu = Factor(1.0/M,MMULT(TRANSPOSE(ones), ror));

    for(int i = 0; i < ror.size(); ++i){
        for(int j = 0; j < ror[i].size(); ++j){
            ror[i][j] -= mu[0][j];
        }
    }

    std::vector<std::vector<double>> Covariance, SD, Correlation;
    Covariance = Factor(1.0/(M - 1), MMULT(TRANSPOSE(ror), ror));

    for(int i = 0; i < N; ++i){
        SD.push_back({sqrt(Covariance[i][i])});
    }
    
    SD = MMULT(SD, TRANSPOSE(SD));

    for(int i = 0; i < N; ++i){
        hold.clear();
        for(int j = 0; j < N; ++j){
            hold.push_back(Covariance[i][j]/SD[i][j]);
        }
        Correlation.push_back(hold);
    }

    std::cout << "\t";
    for(auto & tick : tickers){
        std::cout << tick << "\t";
    }
    std::cout << std::endl;
    for(int i = 0; i < tickers.size(); ++i){
        std::cout << tickers[i] << "\t";
        for(int j = 0; j < N; ++j){
            double tstat = (Correlation[i][j]*sqrt(M - 2))/sqrt(1.0 - pow(Correlation[i][j], 2));
            if(tstat <= 0.01){
                std::cout << Correlation[i][j] << "*\t";
            } else {
                std::cout << Correlation[i][j] << "\t";
            }
        }
        std::cout << std::endl;
    }


    return 0;
}
