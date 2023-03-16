#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    set<int> duplicates;
    map<set<string>, int> docs;
    for (auto id = search_server.begin(); id != search_server.end(); ++id) {
        const auto& doc = search_server.GetWordFrequencies(*id);
        set<string> doc_words;

        for (const auto& [word, _] : doc) {
            doc_words.insert(word);
        }
        if (docs.count(doc_words)) {
            
            auto it = docs.find(doc_words);
            if(*id>it->second){
            duplicates.insert(*id);
                }
            else{
                duplicates.insert(it->second);
            }
        }
        else {
            docs[doc_words]=*id;
        }
    }



    for (auto id : duplicates) {
        cout << "Found duplicate document id "s << id << endl;
        search_server.RemoveDocument(id);
    }
}
