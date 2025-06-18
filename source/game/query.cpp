#include "query.h"
#include "cube.h"

namespace query
{
    // check if a label matches a query string
    bool match(const char *query, const char *label)
    {
        static string qs, ls;
        if(!query || !strlen(query)) return true;  // empty query matches everything
        if(!label || !strlen(label)) return false; // empty label only matches empty query
        for(const char *qp = query; *qp;)
        {
            // find the next word in the query
            while(*qp && *qp == ' ') ++qp;
            if(!*qp) break;
            const char *qword = qp;
            while(*qp && *qp != ' ') ++qp;
            int qlen = qp - qword;
            copystring(qs, qword, qlen+1);

            bool found = false;

            for(const char *lp = label; *lp;)
            {
                // find the next word in the label
                while(*lp && *lp == ' ') ++lp;
                if(!*lp) break;
                const char *lword = lp;
                while(*lp && *lp != ' ') ++lp;
                int llen = lp - lword;
                copystring(ls, lword, llen+1);

                // check if the words match
                if(!strcmp(qs, ls))
                {
                    found = true;
                    break;
                }
            }
            if(!found) return false; // this query word was not found: the entity does not match
        }
        return true; // all query words were found
    }
    // (for testing)
    ICOMMAND(matchlabel, "ss", (char *query, char *label), intret(match(query, label) ? 1 : 0));
} // namespace query