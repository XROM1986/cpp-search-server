#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    set<int> duplicates;
    set<set<string_view>> docs;
    for (auto i = search_server.begin(); i != search_server.end(); ++i) {
        const auto& doc = search_server.GetWordFrequencies(*i);
        set<string_view> doc_words;

        for (const auto& [word, freq] : doc) {
            doc_words.insert(word);
        }
        if (docs.count(doc_words)) {
            duplicates.insert(*i);
        }
        else {
            docs.insert(doc_words);
        }
    }



    for (auto id : duplicates) {
        cout << "Found duplicate document id "s << id << endl;
        search_server.RemoveDocument(id);
    }
}
