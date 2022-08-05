#ifndef __AC_AUTOMATON_H__
#define __AC_AUTOMATON_H__

#include <string>
#include <vector>
#include <utility>
#include <cassert>
#include <queue>

/**
 * @struct AC_automaton PdSupTokenizer.h
 * @brief Tokenizer, using AC automaton internal.
 *
 * @tparam Entry, the basic type to used in Trie
 * @tparam Input, a set of Entry, must have some attributes:
 *              1. begin() and end() exist
 *              2. the iterator obtained from begin() or end() is like someone with std::random_access_iterator_tag
 *              3. constructor Input(first_iterator, second_iterator) exists
 */
template <
    typename Entry = char,
    typename Input = std::basic_string<Entry>>
struct AC_automaton
{
    // nodes that make up the Trie
    struct node
    {
        int id;
        int mark; // > 0 means the node is the end of a token of length [mark]
        std::map<Entry, node *> edge;
    };

    /**
     * recode_point recodes where token appear
     * for exampl: patterns is %1, %2, str is --%1__%2@@, there will be 2 recode_point,
     *      first one is {3, a node}, {7, another node}
     */
    typedef std::pair<size_t, node *> recode_point;

    AC_automaton() : id(0)
    {
        trie.push_back(new node{0});
        next.push_back(0);
    }

    ~AC_automaton()
    {
        for (auto i : trie)
            delete i;
    }

    void build_trie(const std::vector<Input> &patterns)
    {
        for (auto i : patterns)
        {
            node *p = trie.front();
            for (auto c : i)
            {
                if (p->edge.find(c) == p->edge.end())
                {
                    trie.push_back(new node{++id, false});
                    p->edge[c] = trie.back();
                }
                p = p->edge[c];
            }

            p->mark = i.size();
        }
    }

    void generate_next()
    {
        next.clear();
        next.resize(trie.size());

        next[0] = 0;
        std::queue<node *> q;

        // the second layer must ref to root which means its next is 0, so just push the second layer here
        for (auto e : trie.front()->edge)
            q.push(e.second);

        while (!q.empty())
        {
            auto cur_node = q.front();
            q.pop();

            for (auto e : cur_node->edge)
            {
                auto p = trie[next[cur_node->id]];
                while (p->id && p->edge.find(e.first) == p->edge.end())
                    p = trie[next[p->id]]; // jump, until p is root or p has a edge with e

                // If the p has a edge, mark the e.second. Otherwisr, make it ref to root (0)
                next[e.second->id] = p->edge.find(e.first) != p->edge.end() ? p->edge[e.first]->id : 0;

                q.push(e.second);
            }
        }
    }

    std::vector<std::pair<Input, bool>> tokenize(const Input &str, bool greedy = true)
    {
        std::vector<recode_point> token_nodes;

        node *p = trie.front();
        recode_point pre = {0, nullptr};
        for (auto sit = str.begin(); sit != str.end(); ++sit)
        {
            auto it = p->edge.find(*sit);
            if (it == p->edge.end())
            { // need jump
                if (pre.second)
                {
                    token_nodes.push_back(pre);
                    pre.second = nullptr;
                }

                while (p->id && p->edge.find(*sit) == p->edge.end())
                    p = trie[next[p->id]];

                if (p->edge.find(*sit) != p->edge.end())
                    p = p->edge[*sit];

                if (p->mark > 0)
                {
                    pre.first = sit - str.begin();
                    pre.second = p;

                    if (!greedy)
                    {
                        token_nodes.push_back(pre);
                        pre.second = nullptr;
                        p = trie.front();
                    }
                }
            }
            else
            {
                p = it->second;
                if (p->mark > 0)
                {
                    pre.first = sit - str.begin();
                    pre.second = p;

                    if (!greedy)
                    {
                        token_nodes.push_back(pre);
                        pre.second = nullptr;
                        p = trie.front();
                    }
                }
            }
        }
        if (pre.second)
            token_nodes.push_back(pre);

        std::vector<std::pair<Input, bool>> tokens;
        size_t k = 0;
        for (auto i : token_nodes)
        {

            size_t pos = i.first - i.second->mark + 1;
            if (pos > k)
                tokens.push_back(std::make_pair(Input(k + str.begin(), pos + str.begin()), false));
            tokens.push_back(std::make_pair(Input(pos + str.begin(), i.second->mark + pos + str.begin()), true));
            k = i.first + 1;
        }

        assert(k <= str.size());

        if (k < str.size())
            tokens.push_back(std::make_pair(Input(k + str.begin(), str.end()), false));

        return tokens;
    }

    std::vector<node *> trie;
    int id = 0;
    std::vector<int> next;
};

#endif // __AC_AUTOMATON_H__