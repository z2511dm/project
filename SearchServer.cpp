#include <iostream>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>
#include <optional>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

//СЧИТЫВАЕТ И ВОЗВРАЩАЕТ СТРОКУ С ПОТОКА
string ReadLine()
{
    string s;
    getline(cin, s);
    return s;
}

//СЧИТЫВАЕТ И ВОЗВРАЩАЕТ ЧИСЛО С ПОТОКА
int ReadLineWithNumber()
{
    int result;
    cin >> result;
    ReadLine();
    return result;
}

//РАЗБИВАЕТ СТРОКУ НА СЛОВА И ВОЗВРАЩАЕТ ВЕКТОР СЛОВ
vector<string> SplitIntoWords(const string& text)
{
    vector<string> words;
    string word;
    for (const char ch : text)
    {
        if (ch == ' ')
        {
            if (!word.empty())
            {
                words.push_back(word);
                word.clear();
            }
        }
        else
        {
            word += ch;
        }
    }
    if (!word.empty())
    {
        words.push_back(word);
    }
    return words;
}

//ХАРАКТЕРИСТИКИ ДОКУМЕНТА
struct Document
{
    Document() = default;
    Document(int n_id, double n_relevance, int n_rating)
        :id(n_id), relevance(n_relevance), rating(n_rating)
    {
    }
    int id = 0;
    double relevance = 0;
    int rating = 0;
};

//СТАТУСЫ ДОКУМЕНТА
enum class DocumentStatus
{
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

//КЛАСС ПОИСКОВОГО СЕРВЕРА
class SearchServer
{
public:
    
    explicit SearchServer(const string& text) : SearchServer(SplitIntoWords(text))
    {
    }

    template <typename StringCollection>
    explicit SearchServer(const StringCollection& stop_words)
    {
        for (const string& word : stop_words)
        {
            if (!IsValidWord(word))
            {
                throw invalid_argument("invalid characters in stop-word: "s + word);
            }
            stop_words_.insert(word);
        }
    }


    //ДОБАВЛЯЕТ ДОКУМЕНТ БЕЗ СТОП-СЛОВ В ИНВЕРТИРОВАННЫЙ ИНДЕКС
    void AddDocument(int document_id,
                     const string& document,
                     DocumentStatus status,
                     const vector<int>& ratings)
    {
        if (documents_.count(document_id) > 0)
        {
            throw invalid_argument("the document_id is already in the database"s);
        }
        if (document_id < 0)
        {
            throw invalid_argument("document_id < 0"s);
        }
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words)
        {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
        id_index_.push_back(document_id);
    }

    int GetDocumentId(int index) const
    {
        return id_index_.at(index);
    }

    //ВОЗВРАЩАЕТ ТОП-5 ДОКУМЕНТОВ ПО ЗАДАННОЙ ФУНКЦИИ
    template <typename T>
    vector<Document> FindTopDocuments(const string& raw_query, T func) const
    {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, func);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs)
             {
                 const double maximum_error = 1e-6;
                 if (abs(lhs.relevance - rhs.relevance) < maximum_error)
                 {
                     return lhs.rating > rhs.rating;
                 }
                 return lhs.relevance > rhs.relevance;
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT)
        {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query,
                                      DocumentStatus query_status) const
    {
        return FindTopDocuments(raw_query,
                                [query_status](int document_id, DocumentStatus status, int rating)
                                {
                                    return status == query_status;
                                });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const
    {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    //СЧИТАЕТ И ВОЗВРАЩАЕТ КОЛИЧЕСТВО ДОКУМЕНТОВ В БАЗЕ
    int GetDocumentCount() const
    {
        return static_cast<int> (documents_.size());
    }

    //ПОИСК ПЛЮС-СЛОВ И МИНУС-СЛОВ ЗАПРОСА В ДОКУМЕНТЕ
    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
                                                                  int document_id) const
    {
        vector<string> matched_words;
        const Query query = ParseQuery(raw_query);
        for (const string& word : query.plus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id))
            {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id))
            {
                matched_words.clear();
                break;
            }
        }
        return { matched_words, documents_.at(document_id).status };
    }

private:
    //ИНФОРМАЦИЯ ПО ДОКУМЕНТУ
    struct DocumentData
    {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;  //МНОЖЕСТВО СТОП-СЛОВ
    map<string, map<int, double>> word_to_document_freqs_;  //ИНВЕРТИРОВАННЫЙ ИНДЕКС
    map<int, DocumentData> documents_;  //СЛОВАРЬ ИНФОРМАЦИИ О ДОКУМЕНТАХ
    vector<int> id_index_;  //ПОРЯДОК ДОБАВЛЕНИЯ ДОКУМЕНТОВ
    
    //ОПРЕДЕЛЕНИЕ СТРОКИ С НЕДОПУСТИМЫМИ СИМВОЛАМИ
    static bool IsValidWord(const string& word)
    {
        return none_of(word.begin(), word.end(),
                       [](char c)
                       {
                           return c >= '\0' && c < ' ';
                       });
    }

    //ОПРЕДЕЛЯЕТ СТОП-СЛОВА
    bool IsStopWord(const string& word) const
    {
        return stop_words_.count(word) > 0;
    }

    //РАЗБИВАЕТ СТРОКУ НА СЛОВА И ИСКЛЮЧАЕТ ИЗ НИХ СТОП-СЛОВА
    vector<string> SplitIntoWordsNoStop(const string& text) const
    {
        vector<string> words;
        for (const string& word : SplitIntoWords(text))
        {
            if (!IsValidWord(word))
            {
                throw invalid_argument("invalid characters in word: "s + word);
            }
            if (!IsStopWord(word))
            {
                words.push_back(word);
            }
        }
        return words;
    }

    //ВЫЧИСЛЯЕТ СРЕДНИЙ РЕЙТИНГ ДОКУМЕНТА
    static int ComputeAverageRating(const vector<int>& ratings)
    {
        if (ratings.empty())
        {
            return 0;
        }
        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
        return rating_sum / static_cast<int>(ratings.size());
    }
    
    //ИНФОРМАЦИЯ О СЛОВЕ ЗАПРОСА
    struct QueryWord
    {
        string data;
        bool is_minus;
        bool is_stop;
    };

    //ОПРЕДЕЛЯЕТ ТИП СЛОВА ЗАПРОСА
    QueryWord ParseQueryWord(string text) const
    {
        if (!IsValidWord(text))
        {
            throw invalid_argument("invalid characters in query word: "s + text);
        }
        if (text[0] == '-' && text.size() == 1)
        {
            throw invalid_argument("no word after \"-\" in query"s);
        }
        if (text[0] == '-' && text[1] == '-')
        {
            throw invalid_argument("\"--\" in query"s);
        }
        bool is_minus = false;
        if (text[0] == '-')
        {
            is_minus = true;
            text = text.substr(1);
        }
        return { text, is_minus, IsStopWord(text) };
    }

    //СОСТАВ ЗАПРОСА
    struct Query
    {
        set<string> plus_words;
        set<string> minus_words;
    };

    //РАЗБИВАЕТ СТРОКУ ЗАПРОСА НА СЛОВА, ИСКЛЮЧАЕТ СТОП-СЛОВА
    Query ParseQuery(const string& text) const
    {
        Query query;
        for (const string& word : SplitIntoWords(text))
        {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop)
            {
                if (query_word.is_minus)
                {
                    query.minus_words.insert(query_word.data);
                }
                else
                {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    //ВЫЧИСЛЯЕТ IDF
    double ComputeWordInverseDocumentFreq(const string& word) const
    {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    //ВОЗВРАЩАЕТ ID И TF-IDF ДЛЯ КАЖДОГО ДОКУМЕНТА, ГДЕ НАЙДЕНЫ СЛОВА
    template <typename T>
    vector<Document> FindAllDocuments(const Query& query, T func) const
    {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word))
            {
                const auto& document = documents_.at(document_id);
                if (func(document_id, document.status, document.rating))
                {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            for (const auto& [document_id, _] : word_to_document_freqs_.at(word))
            {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto& [document_id, relevance] : document_to_relevance)
        {
            matched_documents.push_back({ document_id,
                                          relevance,
                                          documents_.at(document_id).rating });
        }
        return matched_documents;
    }
};


void PrintDocument(const Document& document)
{
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}
int main()
{
    try
    {
        SearchServer search_server ("ag\x12g"s);
        // Явно игнорируем результат метода AddDocument, чтобы избежать предупреждения
        // о неиспользуемом результате его вызова
        search_server.AddDocument (1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        search_server.AddDocument (1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
        search_server.AddDocument (-1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
        search_server.AddDocument (3, "большой пёс скво\x12рец"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
        const auto documents = search_server.FindTopDocuments ("--пушистый"s);
        for (const Document& document : documents)
        {
            PrintDocument(document);
        }
    }
    catch (const invalid_argument& e)
    {
        cout << "Error: "s << e.what () << endl;
    }
    catch (const out_of_range& e)
    {
        cout << "Error: "s << e.what () << endl;
    }
}
