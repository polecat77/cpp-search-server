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

// -------- ������ ��������� ������ ��������� ������� ----------

// ���� ���������, ��� ��������� ������� ��������� ����-����� ��� ���������� ����������
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {//�������� ���������� ��������� � ���������� ��� �������.
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {//�������� ����� ���� ����
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

// ���� ��������� ������������ ���������� ���������� �������.
void TestSearchServerMatched(){
    const int doc_id = 0;
    const string content = "����� ��� � ������ �������"s;
    const vector<int> ratings = {8, -3};
    {
        //�������� ��� ���� ����
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const int document_count = server.GetDocumentCount();
        for (int document_id = 0; document_id < document_count; ++document_id) {
            const auto [words, status] = server.MatchDocument("�������� ���"s, document_id);
            ASSERT_EQUAL(words.size(), 1);
            ASSERT_EQUAL(document_id, 0);
            ASSERT_EQUAL(status, DocumentStatus::ACTUAL);
        }
    }
    {
        //�������� �� ���� �������
        SearchServer server;
        server.SetStopWords("���"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const int document_count = server.GetDocumentCount();
        for (int document_id = 0; document_id < document_count; ++document_id) {
            const auto [words, status] = server.MatchDocument("�������� ���"s, document_id);
            ASSERT_EQUAL(words.size(), 0);
            ASSERT_EQUAL(document_id, 0);
            ASSERT_EQUAL(status, DocumentStatus::ACTUAL);
        }
    }
    {
        //�������� � ����� �������
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const int document_count = server.GetDocumentCount();
        for (int document_id = 0; document_id < document_count; ++document_id) {
            const auto [words, status] = server.MatchDocument("�������� -���"s, document_id);
            ASSERT_EQUAL(words.size(), 0); //��������, ��� ����� ������
        }
    }
    
}

// �������� ���������� �� �������������
void TestSearchServerRelevanse(){
    SearchServer server;
    server.SetStopWords("� � ��"s);
    server.AddDocument(0, "����� ��� � ������ �������"s,        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "�������� ��� �������� �����"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    server.AddDocument(3, "��������� ������� �������"s,         DocumentStatus::BANNED, {9});

    const auto& documents = server.FindTopDocuments("�������� ��������� ���"s);
    int doc_size = documents.size();
    ASSERT_EQUAL(doc_size, 3); //��������, ��� ����� �� ������ ��������� � ���������
    ASSERT_HINT(is_sorted(documents.begin(), documents.end(),
            [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                     return lhs.rating > rhs.rating;
                 } else {
                     return lhs.relevance > rhs.relevance;
                 }
             }), "Relevance not sorted correctly"s);
}

// �������� ������������ �������� ��������
void TestSearchServerRating(){
    //������� ������ � ����������
    vector<vector<int>> ratings = {{8, -3}, {7, 2, 7}, {5, -12, 2, 1}, {9}};
    //��������� �������� � �������� � ������
    map<int, int> rating_count;
    for(int i = 0; i < ratings.size(); ++i){
        rating_count[i] = (accumulate(ratings[i].begin(), ratings[i].end(), 0) / ratings[i].size()); // ��������� ������� ������� � ����������� � id
    }

    SearchServer server;
    //server.SetStopWords("� � ��"s);
    server.AddDocument(0, "����� ��� � ������ �������"s,        DocumentStatus::ACTUAL, ratings[0]); // ������� ����������� �� ������ 2
    server.AddDocument(1, "�������� ��� �������� �����"s,       DocumentStatus::ACTUAL, ratings[1]); // ������� ����������� �� ������ 5
    server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::ACTUAL, ratings[2]); // ������� ����������� �� ������ -1
    server.AddDocument(3, "��������� ������� �������"s,         DocumentStatus::BANNED, ratings[3]); // ������� 9

    const auto& documents = server.FindTopDocuments("�������� ��������� ���"s);
    int doc_size = documents.size();
    for(int i =0; i < doc_size; ++i){
        ASSERT_HINT(documents[i].rating == rating_count[documents[i].id], "The rating is calculated incorrectly"s);
    }
}

//�������� ������ �� ������� ���������
void TestSearchServerStatus(){
    SearchServer server;
    server.SetStopWords("� � ��"s);
    server.AddDocument(0, "����� ��� � ������ �������"s,        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "�������� ��� �������� �����"s,       DocumentStatus::IRRELEVANT, {7, 2, 7});
    server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::BANNED, {5, -12, 2, 1});
    server.AddDocument(3, "��������� ������� �������"s,         DocumentStatus::REMOVED, {9});
    {// ������� ������� ������ ��������� �� �������� ACTUAL
        const auto& documents = server.FindTopDocuments("�������� ��������� ���"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(documents.size(), 1);
    }
    {// ������� ������� ������ ��������� �� �������� IRRELEVANT
        const auto& documents = server.FindTopDocuments("�������� ��������� ���"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(documents.size(), 1);
    }
    {// ������� ������� ������ ��������� �� �������� BANNED
        const auto& documents = server.FindTopDocuments("�������� ��������� ���"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(documents.size(), 1);
    }
    {// ������� ������� ������ ��������� �� �������� REMOVED
        const auto& documents = server.FindTopDocuments("�������� ��������� ���"s, DocumentStatus::REMOVED);
        ASSERT_EQUAL(documents.size(), 1);
    }
    {// ������� ������� ������ ��������� �� �������� �� ��������� (ACTUAL)
        const auto& documents = server.FindTopDocuments("�������� ��������� ���"s);
        ASSERT_EQUAL(documents.size(), 1);
    }
}

void TestSearchServerPredictate(){
    SearchServer server;
    server.SetStopWords("� � ��"s);
    server.AddDocument(0, "����� ��� � ������ �������"s,        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "�������� ��� �������� �����"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    server.AddDocument(3, "��������� ������� �������"s,         DocumentStatus::BANNED, {9});
    { //�������� id ���������
        const auto& documents = server.FindTopDocuments("�������� ��������� ���"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
        for(const auto& document : documents){
            ASSERT_HINT(document.id % 2 == 0, ""s);
        }
    }
    { //�������� �������
        const auto& documents = server.FindTopDocuments("�������� ��������� ���"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL;});
            ASSERT_EQUAL(documents.size(), 3);
    }
    { //�������� ��������
        const auto& documents = server.FindTopDocuments("�������� ��������� ���"s, [](int document_id, DocumentStatus status, int rating) { return rating > 3;});
        for(const auto& document : documents){
            ASSERT_HINT(document.rating > 3, "Rating does not match"s);
        }
    }
}

void TestSearchServerMinus(){
    SearchServer server;
    server.SetStopWords("� � ��"s);
    server.AddDocument(0, "����� ��� � ������ �������"s,        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "�������� ��� �������� �����"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    server.AddDocument(3, "��������� ������� �������"s,         DocumentStatus::BANNED, {9});
    const auto& documents = server.FindTopDocuments("�������� -��������� -���"s);
    ASSERT_EQUAL(documents.size(), 0);
}

void TestSearchServerCalcRelevance(){
    SearchServer server;
    //������������� ������������ � ��������� ���������
    map< int, double> calc_freq = {{0, 0.173286}, {1, 0.866433}, {2, 0.173286}, {3, 0.173286}};
    server.SetStopWords("� � ��"s);
    server.AddDocument(0, "����� ��� � ������ �������"s,        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "�������� ��� �������� �����"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    server.AddDocument(3, "��������� ������� �������"s,         DocumentStatus::BANNED, {9});
    const auto& documents = server.FindTopDocuments("�������� ��������� ���"s);
    for (const Document& document : documents) {
        ASSERT(abs(document.relevance - calc_freq.at(document.id)) < 1e-6);
    }
}



// ������� TestSearchServer �������� ������ ����� ��� ������� ������
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
    // ���� �� ������ ��� ������, ������ ��� ����� ������ �������
    cout << "Search server testing finished"s << endl;
}
