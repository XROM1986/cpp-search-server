#include "search_server.h"

using std::operator ""s;
//Конструкторы
SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))
    {
    }
    
    SearchServer::SearchServer(std::string_view stop_words_text)
         :SearchServer(SplitIntoWords(stop_words_text))
    {
    }
   
//Добавление нового документа
   void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status,
                     const std::vector<int>& ratings) {
        if ((document_id < 0) || (documents_.count(document_id) > 0)) {
            throw std::invalid_argument("Invalid document_id"s);
        }
    documents_.emplace(document_id, DocumentData{ SearchServer::ComputeAverageRating(ratings), status});
       
       document_text_.emplace(document_id, std::string(document));
    
    auto words = SplitIntoWordsNoStop(document_text_.at(document_id));
 const double inv_word_count = 1.0 / words.size();   
    for (auto word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][word] += inv_word_count;
    }
    
    document_ids_.insert(document_id);     
  
    }

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(std::execution::seq, raw_query, status);
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const {
    return FindTopDocuments(std::execution::seq, raw_query, DocumentStatus::ACTUAL);
}


//Получение частот слов по id документа
    const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const std::map<std::string_view, double> empty_result;

    if (document_to_word_freqs_.count(document_id)) {
        return document_to_word_freqs_.at(document_id);
    }
    else {
        return empty_result;
    }      
}
//Возврат количества документов
    int SearchServer::GetDocumentCount() const {
        return documents_.size();
    }


std::set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}


std::set<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

//Удаление документа из поискового сервера
    void SearchServer::RemoveDocument(int document_id) {
        
   if (document_ids_.find(document_id) == document_ids_.end()){
        return;
    }   
        for (auto [word, freq] : document_to_word_freqs_.at(document_id)) {
        word_to_document_freqs_.at(word).erase(document_id);
    }
    document_to_word_freqs_.erase(document_id);
    document_ids_.erase(document_id);
 documents_.erase(document_id);
}


//Возврат списка совпавших слов запроса
SearchServer::ResultMatchDocument SearchServer::MatchDocument( std::string_view raw_query, int document_id) const {
   const Query query = ParseQuery(raw_query);

    std::vector<std::string_view> matched_words;
    for (const std::string_view word : query.minus_words) {
     
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
             return { {}, documents_.at(document_id).status };
        }
    }
    for(const std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    

    return { matched_words, documents_.at(document_id).status };  
      

}

SearchServer::ResultMatchDocument SearchServer::MatchDocument(const std::execution::sequenced_policy&  policy,std::string_view raw_query, int document_id) const {
    
        return MatchDocument(raw_query, document_id);

}

SearchServer::ResultMatchDocument SearchServer::MatchDocument(const std::execution::parallel_policy& policy, std::string_view raw_query, int document_id) const {
    if ((document_id < 0) || (documents_.count(document_id) == 0)) {
            throw std::invalid_argument("document_id out of range"s);
        }
    
    
    const Query& query = ParseQueryParallel(raw_query);
    
std::vector<std::string_view> matched_words;
 matched_words.reserve(query.plus_words.size()); 
    const auto& word_freqs = document_to_word_freqs_.at(document_id);
    
    if (any_of(policy,query.minus_words.begin(),
                    query.minus_words.end(),
                    [&word_freqs](const std::string_view word) {
                        return word_freqs.count(word) > 0;
                    })) {
        return { {}, documents_.at(document_id).status };
    }

    copy_if( policy, query.plus_words.begin(),
                 query.plus_words.end(),
                 back_inserter(matched_words),
                 [&word_freqs](const std::string_view word) {
                     return word_freqs.count(word) > 0;
                 });

sort(policy, matched_words.begin(), matched_words.end());
    const auto& itr = unique(matched_words.begin(), matched_words.end());
    matched_words.erase(itr, matched_words.end());
    //
    return { matched_words, documents_.at(document_id).status };
} 

//Проверка входящего слова на принадлежность к стоп-словам
  bool SearchServer::IsStopWord( std::string_view word) const {
        return stop_words_.count(word) > 0;
    }

//Проверка на отсутствие в слове спецсимволов
    bool SearchServer::IsValidWord(std::string_view word) {
        return std::none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        });
    }

//Разбивка строки запроса на вектор слов, исключая стоп-слова
    std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const {
    	std::vector<std::string_view> words;
        for (const std::string_view word : SplitIntoWords(text)) {
            if (!IsValidWord(word)) {
                throw std::invalid_argument("Недопустимое слово"s);
            }
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

//Подсчет среднего рейтинга
    int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

//Удалеие "-" у минус-слов
    SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
        if (text.empty()) {
            throw std::invalid_argument("Пустое слово"s);
        }
        std::string_view word = text;
        bool is_minus = false;
        if (word[0] == '-') {
            is_minus = true;
            word = word.substr(1);
        }
        if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
            throw std::invalid_argument("Наличие более одного минуса"s);
        }
        return {word, is_minus, IsStopWord(word)};
    }

//Создание списков плюс- и минус-слов

   SearchServer::Query SearchServer::ParseQuery(std::string_view text) const {
        Query result;
    for (const std::string_view word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            }
            else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    sort(result.minus_words.begin(), result.minus_words.end());
    auto itm = unique(result.minus_words.begin(), result.minus_words.end());
    result.minus_words.resize(distance(result.minus_words.begin(), itm));
    
    sort(result.plus_words.begin(), result.plus_words.end());
    auto itp = unique(result.plus_words.begin(), result.plus_words.end());
    result.plus_words.resize(distance(result.plus_words.begin(), itp));
    
    return result;
    }
    
SearchServer::Query SearchServer::ParseQueryParallel(std::string_view text) const {
    Query result;
    for (const std::string_view word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            }
            else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    return result;
}

//Вычисление IDF слова
    double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }