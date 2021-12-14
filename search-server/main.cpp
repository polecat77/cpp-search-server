class SearchServer {
public:

    inline static constexpr int INVALID_DOCUMENT_ID = -1;
    inline static constexpr double EPSILON = 1e-6;
    
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words) 
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
        for (const auto& word : stop_words_) {
            if (!IsValidWord(word)) {
                throw invalid_argument("недопустимые символы в стоп-слове \""s + word+ "\""s);
            }
        }
    }
    
    explicit SearchServer(const string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
    {
        for (const auto& word : stop_words_) {
            if (!IsValidWord(word)) {
                throw invalid_argument("недопустимые символы в стоп-слове \""s + word + "\""s);
            }
        }
    }
    
    //Добавляем документы
    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        if (document_id < 0) {
            throw invalid_argument("id документа \""s + document + "\" меньше нуля"s);
        }
        if (documents_.count(document_id) > 0) {
            throw invalid_argument("документ с данным id уже добавлен \""s + document + "\""s);
        }        
        if (!IsValidWord(document)) {
            throw invalid_argument("недопустимые символы в документе \""s + document + "\""s);
        }        
        
        vector<string> words;
        if (!SplitIntoWordsNoStop(document, words)) {
            throw invalid_argument("попытка разбития документа \""s + document + "\" на слова закончилась неудачно"s);
        }

        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
        document_ids_.push_back(document_id);
    }
    //Ищем в документах
    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
    //Проверка условий
        if (IsNoValidQuery(raw_query)) {
            throw invalid_argument("некорректный поисковый запрос \""s + raw_query + "\""s);
        }
        Query query;
        //Разбираем запрос и в случае неудачи выдаем исключение
        if (!ParseQuery(raw_query, query)) {
            throw invalid_argument("попытка разбития на слова поискового запроса \""s + raw_query + "\" закончилась неудачно "s);
        }
        auto matched_documents = FindAllDocuments(query, document_predicate);
        sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
            if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
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
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
                return document_status == status;
        });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    int GetDocumentId(int index) const {
        if (index < 0 || index > GetDocumentCount()) {
            throw out_of_range("id не найден"s);
        }
        return document_ids_[index];
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        //Проверка условий
        if (IsNoValidQuery(raw_query)) {
            throw invalid_argument("некорректный поисковый запрос \""s + raw_query + "\""s);
        }
        
        Query query;
        if (!ParseQuery(raw_query, query)) {
            throw invalid_argument("попытка разбития на слова матчинг запроса закончилась неудачно "s);
        }
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

    const set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> document_ids_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    //Проверка слов на использование спецсимволов
    static bool IsValidWord(const string& word) {
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        });
    }
    //Проверка запроса на некорректные минусы
    static bool IsNotValidQuery(const string& raw_query) {
        if (raw_query[raw_query.size() - 1] == '-') return false;
        const vector <string> bad_symbols = {"--", " - ", "- "};
        for (const string& temp : bad_symbols) {
            if (raw_query.find(temp) == 0) {
                return false;
            }
        }
        return true;
    }

    static bool IsNoValidQuery(const string& query) {
        if (!IsNotValidQuery(query) || !IsValidWord(query)) {
            return true;
        }
        return false;
    }

    [[nodiscard]] bool SplitIntoWordsNoStop(const string& text, vector<string>& result) const {
        result.clear();
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsValidWord(word)) {
                return false;
            }
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        result.swap(words);
        return true;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    [[nodiscard]] bool ParseQueryWord(string text, QueryWord& result) const {
        // Empty result by initializing it with default constructed QueryWord
        result = {};
        if (text.empty()) {
            return false;
        }
        bool is_minus = false;
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
            return false;
        }
        result = QueryWord{text, is_minus, IsStopWord(text)};
        return true;
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    [[nodiscard]] bool ParseQuery(const string& text, Query& result) const {
        // Empty result by initializing it with default constructed Query
        result = {};
        for (const string& word : SplitIntoWords(text)) {
            QueryWord query_word;
            if (!ParseQueryWord(word, query_word)) {
                return false;
            }
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    result.minus_words.insert(query_word.data);
                } else {
                    result.plus_words.insert(query_word.data);
                }
            }
        }
        return true;
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
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
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
            matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};
