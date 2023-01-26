#include <algorithm>
#include <iostream>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <numeric>
#include <vector>

//#include "search_server.h"

using namespace std;

/* Подставьте вашу реализацию класса SearchServer сюда */
const int MAX_RESULT_DOCUMENT_COUNT = 5;
const int MIN_DELTA = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
                     const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < MIN_DELTA) {
                    return lhs.rating > rhs.rating;
                } else {
                    return lhs.relevance > rhs.relevance;
                }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }


    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, [&status](int document_id, DocumentStatus status_new, int rating) {return status_new == status;});
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);

    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
                                                        int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = accumulate(ratings.begin(), ratings.end(),0);

        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
    map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                DocumentData documents_data = documents_.at(document_id);
                if (document_predicate(document_id, documents_data.status, documents_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

/*
   Подставьте сюда вашу реализацию макросов
   ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST*/
#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

#define ASSERT(expr) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

#define RUN_TEST(func) RunTestImpl((func), #func) // напишите недостающий код

   template <typename T>
ostream& operator << (ostream &out, const vector<T> &data){
    out<<"["s;
    bool flag=true;
    for(const T i:data){
        if(!flag)
            out<<", "s;
        out<<i;
        flag=false;
        }
    out<<"]"s;
    return out;
    }

template <typename T>
ostream& operator << (ostream &out, const set<T> &data){
    out<<"{"s;
    bool flag=true;
    for(const T i:data){
        if(!flag)
            out<<", "s;
        out<<i;
        flag=false;
        }
    out<<"}"s;
    return out;
    }

template <typename A, typename B>
ostream& operator << (ostream &out, const map<A,B> &data){
    out<<"{"s;
    bool flag=true;
    for(const auto i:data){
        if(!flag)
            out<<", "s;
        out<<i.first<<": "s<<i.second;
        flag=false;
        }
    out<<"}"s;
    return out;
    }

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}


void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}



template <typename F>
void RunTestImpl(F func, const string& func_str) {
    func();
    cerr << func_str << " OK" << endl;
}





// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Stop words must be excluded from documents"s);
    }
}

/*
Разместите код остальных тестов здесь
*/ void TestAddedDocumentFindWord() {
    const int id = 15;
    const DocumentStatus status=DocumentStatus::ACTUAL;
    const string content = "тише мыши"s;
    const vector<int> ratings = { 1, 2, 3 };

    {
        SearchServer server;
        ASSERT_EQUAL(server.GetDocumentCount(), 0);
        server.AddDocument(id, content, status, ratings);
        server.AddDocument(16, "кот на крыше", status, ratings);
        ASSERT_EQUAL(server.GetDocumentCount(), 2);
        const auto found_docs = server.FindTopDocuments("мыши"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, id);
    }
}
//Поддержка минус-слов. Документы, содержащие минус-слова из поискового запроса, не должны включаться в результаты поиска.
void TestSupportMinusWords() {
    SearchServer server;
    server.AddDocument(101, "бабочка красавица, кушайте варенье"s, DocumentStatus::ACTUAL, { 1,2,3 });
    server.AddDocument(102, "муха не красавица на базар"s, DocumentStatus::ACTUAL, { 1,2,3 });

        const auto found_docs1 = server.FindTopDocuments("варенье -муха"s);
        ASSERT_EQUAL(found_docs1.size(), 1);
        ASSERT_EQUAL(found_docs1[0].id, 101);

        const auto found_docs2 = server.FindTopDocuments(" базар -бабочка"s);
        ASSERT_EQUAL(found_docs2.size(), 1);
        ASSERT_EQUAL(found_docs2[0].id, 102);
    }

//Соответствие документов поисковому запросу.
 void TestMatchedDocuments() {
        SearchServer server;
        server.SetStopWords("и в на"s);
        server.AddDocument(100, "прыгают на улице и радуются"s, DocumentStatus::ACTUAL, { 1, 2, 3 });

            const auto [matched_words1, status1] = server.MatchDocument("прыгают и радуются"s, 100);
            const vector<string> expected_result1 = { "прыгают"s, "радуются"s };
            ASSERT_EQUAL(expected_result1, matched_words1);

            const auto [matched_words2, status2] = server.MatchDocument("прыгают иd -радуются"s, 100);
            const vector<string> expected_result2 = {}; // пустой результат поскольку есть минус-слово
            ASSERT_EQUAL(expected_result2, matched_words2);
          ASSERT(matched_words2.empty());
    }
 // Сортировка найденных документов по релевантности.
 void TestSortRelevance() {
     SearchServer server;
     server.AddDocument(10, "кто ходит в гости по утрам"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
     server.AddDocument(20, "тот поступает мудро"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
     server.AddDocument(30, "рано вставать по утрам"s, DocumentStatus::ACTUAL, { 1, 2, 3 });

     {
         const auto found_docs = server.FindTopDocuments("по утрам"s);
         ASSERT_EQUAL(found_docs.size(), 2);
         for (size_t i = 1; i < found_docs.size(); i++) {
             ASSERT(found_docs[i - 1].relevance >= found_docs[i].relevance);
         }
     }
 }

 // Вычисление рейтинга документов.
 void TestCalculateRating() {
     SearchServer server;
     const vector<int> ratings = { 10, 11, 3 };
     const int average = (10 + 11 + 3) / 3;
     server.AddDocument(25, "бабочка красавица, кушайте варенье"s, DocumentStatus::ACTUAL, ratings);

     {
         const auto found_docs = server.FindTopDocuments("бабочка красавица"s);
         ASSERT_EQUAL(found_docs.size(), 1);
         ASSERT_EQUAL(found_docs[0].rating, average);
     }
 }

 // Фильтрация результатов поиска с использованием предиката.
 void TestUsePredicate() {
     SearchServer server;
     server.AddDocument(100, "кот на крыше"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
     server.AddDocument(101, "собака на боку"s, DocumentStatus::IRRELEVANT, { -1, -2, -3 });
     server.AddDocument(102, "обезьяны на лианах"s, DocumentStatus::ACTUAL, { -4, -5, -6 });
     const auto found_docs = server.FindTopDocuments("собака на"s, [](int document_id, DocumentStatus status, int rating) { return rating > 0; });

     {
         ASSERT_HINT(found_docs[0].id == 100, "Неверно");
     }
 }

 // Поиск документов, имеющих заданный статус.
 void TestSearchStatus() {
     const int id1 = 11;
     const int id2 = 12;
     const int id3 = 13;
     const string content1 = "когда рак на горе свистнет"s;
     const string content2 = "красный рак на столе "s;
     const string content3 = "рак любит лавать на животе"s;
     const vector<int> ratings = { 1, 2, 3 };
     SearchServer server;
     server.AddDocument(id1, content1, DocumentStatus::ACTUAL, ratings);
     server.AddDocument(id2, content2, DocumentStatus::IRRELEVANT, ratings);
     server.AddDocument(id3, content3, DocumentStatus::IRRELEVANT, ratings);
     const auto found_docs = server.FindTopDocuments("на рак"s, DocumentStatus::IRRELEVANT);


         ASSERT_HINT(found_docs[0].id == id2, "Статус неверный");
         ASSERT_HINT(found_docs[1].id == id3, "Статус неверный");
         ASSERT_HINT(found_docs.size() == 2, "Неверный запрос");

 }

 // Корректное вычисление релевантности найденных документов.
 void TestCalculateRelevance() {
     SearchServer server;
     server.AddDocument(10, "снег падал влесу"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
     server.AddDocument(11, " белый снег кружился и таял"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
     server.AddDocument(12, "кот спал на диване"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
     server.AddDocument(15, "белый бим, черное ухо"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
     const auto found_docs = server.FindTopDocuments("белый снег"s);
     double relevance = log(4.0/ 2) * (1.0/ 5) + log(4.0 / 2) * (1.0 /5);

     {
         ASSERT_HINT(fabs(found_docs[0].relevance - relevance) < 1e-6, "Не верный расчет релевантности");
     }
 }


// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddedDocumentFindWord);
    RUN_TEST(TestSupportMinusWords);
    RUN_TEST(TestMatchedDocuments);
    RUN_TEST(TestSortRelevance);
    RUN_TEST(TestCalculateRating);
    RUN_TEST(TestUsePredicate);
    RUN_TEST(TestSearchStatus);
    RUN_TEST(TestCalculateRelevance);
    // Не забудьте вызывать остальные тесты здесь
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}



