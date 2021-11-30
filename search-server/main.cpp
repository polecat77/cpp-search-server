#include <numeric>

ostream& operator<<(ostream& os, const DocumentStatus& ds) {
    switch(static_cast<int>(ds)) {
        case 0 : os << "ACTUAL"s; break;
        case 1 : os << "IRRELEVANT"s; break;
        case 2 : os << "BANNED"s; break;
        case 3 : os << "REMOVED"s; break;
    }
    return os;
}

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {//Проверка добавления документа и нахождении при запросе.
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {//Проверка учета стоп слов
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

// Тест проверяет соответствие документов поисковому запросу.
void TestSearchServerMatched(){
    const int doc_id = 0;
    const string content = "белый кот и модный ошейник"s;
    const vector<int> ratings = {8, -3};
    {
        //Проверка без стоп слов
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const int document_count = server.GetDocumentCount();
        for (int document_id = 0; document_id < document_count; ++document_id) {
            const auto [words, status] = server.MatchDocument("пушистый кот"s, document_id);
            ASSERT_EQUAL(words.size(), 1);
            ASSERT_EQUAL(document_id, 0);
            ASSERT_EQUAL(status, DocumentStatus::ACTUAL);
        }
    }
    {
        //Проверка со стоп словами
        SearchServer server;
        server.SetStopWords("кот"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const int document_count = server.GetDocumentCount();
        for (int document_id = 0; document_id < document_count; ++document_id) {
            const auto [words, status] = server.MatchDocument("пушистый кот"s, document_id);
            ASSERT_EQUAL(words.size(), 0);
            ASSERT_EQUAL(document_id, 0);
            ASSERT_EQUAL(status, DocumentStatus::ACTUAL);
        }
    }
    {
        //Проверка с минус словами
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const int document_count = server.GetDocumentCount();
        for (int document_id = 0; document_id < document_count; ++document_id) {
            const auto [words, status] = server.MatchDocument("пушистый -кот"s, document_id);
            ASSERT_EQUAL(words.size(), 0); //Проверка, что ответ пустой
        }
    }
    
}

// Проверка сортировки по релевантности
void TestSearchServerRelevanse(){
    SearchServer server;
    server.SetStopWords("и в на"s);
    server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    const auto& documents = server.FindTopDocuments("пушистый ухоженный кот"s);
    int doc_size = documents.size();
    ASSERT_EQUAL(doc_size, 3); //Проверка, что ответ по длинне совпадает с ожидаемым
    ASSERT_HINT(is_sorted(documents.begin(), documents.end(),
            [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                     return lhs.rating > rhs.rating;
                 } else {
                     return lhs.relevance > rhs.relevance;
                 }
             }), "Relevance not sorted correctly"s);
}

// Проверка правильности подсчета рейтинга
void TestSearchServerRating(){
    //Создали вектор с рейтингами
    vector<vector<int>> ratings = {{8, -3}, {7, 2, 7}, {5, -12, 2, 1}, {9}};
    //Посчитали рейтинги и положили в вектор
    map<int, int> rating_count;
    for(int i = 0; i < ratings.size(); ++i){
        rating_count[i] = (accumulate(ratings[i].begin(), ratings[i].end(), 0) / ratings[i].size()); // Вычисляем рейтинг вручную и привязываем к id
    }

    SearchServer server;
    //server.SetStopWords("и в на"s);
    server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, ratings[0]); // Рейтинг округляется до целого 2
    server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, ratings[1]); // Рейтинг округляется до целого 5
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, ratings[2]); // Рейтинг округляется до целого -1
    server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, ratings[3]); // Рейтинг 9

    const auto& documents = server.FindTopDocuments("пушистый ухоженный кот"s);
    int doc_size = documents.size();
    for(int i =0; i < doc_size; ++i){
        ASSERT_HINT(documents[i].rating == rating_count[documents[i].id], "The rating is calculated incorrectly"s);
    }
}

//Проверка поиска по статусу документа
void TestSearchServerStatus(){
    SearchServer server;
    server.SetStopWords("и в на"s);
    server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::IRRELEVANT, {7, 2, 7});
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::BANNED, {5, -12, 2, 1});
    server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::REMOVED, {9});
    {// Поверка наличия одного документа со статусом ACTUAL
        const auto& documents = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(documents.size(), 1);
    }
    {// Поверка наличия одного документа со статусом IRRELEVANT
        const auto& documents = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(documents.size(), 1);
    }
    {// Поверка наличия одного документа со статусом BANNED
        const auto& documents = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(documents.size(), 1);
    }
    {// Поверка наличия одного документа со статусом REMOVED
        const auto& documents = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::REMOVED);
        ASSERT_EQUAL(documents.size(), 1);
    }
    {// Поверка наличия одного документа со статусом по умолчанию (ACTUAL)
        const auto& documents = server.FindTopDocuments("пушистый ухоженный кот"s);
        ASSERT_EQUAL(documents.size(), 1);
    }
}

void TestSearchServerPredictate(){
    SearchServer server;
    server.SetStopWords("и в на"s);
    server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});
    { //Проверка id документа
        const auto& documents = server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
        for(const auto& document : documents){
            ASSERT_HINT(document.id % 2 == 0, ""s);
        }
    }
    { //Проверка статуса
        const auto& documents = server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL;});
            ASSERT_EQUAL(documents.size(), 3);
    }
    { //Проверка рейтинга
        const auto& documents = server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return rating > 3;});
        for(const auto& document : documents){
            ASSERT_HINT(document.rating > 3, "Rating does not match"s);
        }
    }
}

void TestSearchServerMinus(){
    SearchServer server;
    server.SetStopWords("и в на"s);
    server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});
    const auto& documents = server.FindTopDocuments("пушистый -ухоженный -кот"s);
    ASSERT_EQUAL(documents.size(), 0);
}

void TestSearchServerCalcRelevance(){
    SearchServer server;
    //Релевантности рассчитанные в сторонней программе
    map< int, double> calc_freq = {{0, 0.173286}, {1, 0.866433}, {2, 0.173286}, {3, 0.173286}};
    server.SetStopWords("и в на"s);
    server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});
    const auto& documents = server.FindTopDocuments("пушистый ухоженный кот"s);
    for (const Document& document : documents) {
        ASSERT(abs(document.relevance - calc_freq.at(document.id)) < 1e-6);
    }
}



// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestSearchServerMatched);
    RUN_TEST(TestSearchServerRelevanse);
    RUN_TEST(TestSearchServerRating);
    RUN_TEST(TestSearchServerStatus);
    RUN_TEST(TestSearchServerPredictate);
    RUN_TEST(TestSearchServerMinus);
    RUN_TEST(TestSearchServerCalcRelevance);

}

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}
