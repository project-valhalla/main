#include "query.h"
#include "cube.h"

namespace game
{
    namespace query
    {
        // check if a label matches a query string
        bool match(const char* query, const char* label)
        {
            static string qs, ls;
            if (!query || !strlen(query))
            {
                // empty query matches everything
                return true;
            }
            if (!label || !strlen(label))
            {
                // empty label only matches empty query
                return false;
            }
            for (const char* qp = query; *qp;)
            {
                // find the next word in the query
                while (*qp && *qp == ' ')
                {
                    ++qp;
                }
                if (!*qp)
                {
                    break;
                }
                const char* qword = qp;
                while (*qp && *qp != ' ')
                {
                    ++qp;
                }
                int qlen = qp - qword;
                copystring(qs, qword, qlen + 1);
                bool found = false;
                for (const char* lp = label; *lp;)
                {
                    // find the next word in the label
                    while (*lp && *lp == ' ')
                    {
                        ++lp;
                    }
                    if (!*lp)
                    {
                        break;
                    }
                    const char* lword = lp;
                    while (*lp && *lp != ' ')
                    {
                        ++lp;
                    }
                    int llen = lp - lword;
                    copystring(ls, lword, llen + 1);
                    if (!strcmp(qs, ls)) // check if the words match
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    // this query word was not found: the entity does not match
                    return false;
                }
            }
            return true; // all query words were found
        }
        // (for testing)
        ICOMMAND(matchlabel, "ss", (char* query, char* label), intret(match(query, label) ? 1 : 0));
    } // namespace query
}