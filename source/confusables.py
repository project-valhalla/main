#!/usr/bin/env python3

import requests

CONFUSABLES_URL = "https://www.unicode.org/Public/security/latest/confusables.txt"

def is_codepoint_allowed(codepoint: int) -> bool:
    return (
        (codepoint >= 0x0021 and codepoint <= 0x007E) # ASCII

        or (codepoint >= 0x00A1 and codepoint <= 0x00AC)
        or (codepoint >= 0x00AE and codepoint <= 0x00FF) # Latin-1 Supplement

        or (codepoint >= 0x0100 and codepoint <= 0x0131)
        or (codepoint >= 0x0134 and codepoint <= 0x0148)
        or (codepoint >= 0x014A and codepoint <= 0x017E) # Latin Extended-A (exclude 0x131, 0x132, 0x149 and 0x17F)

        or (codepoint >= 0x0218 and codepoint <= 0x021B) # Latin Extended-B: Romanian

        or (codepoint == 0x0386)
        or (codepoint >= 0x0388 and codepoint <= 0x038A)
        or (codepoint == 0x038C)
        or (codepoint >= 0x038E and codepoint <= 0x03A1)
        or (codepoint >= 0x03A3 and codepoint <= 0x03CE) # Greek and Coptic

        or (codepoint >= 0x0400 and codepoint <= 0x045F) # Cyrillic

        or (codepoint >= 0x1EA0 and codepoint <= 0x1EF9) # Latin Extended Additional: Vietnamese
    )

confusables = dict()

# download confusables file from Unicode
response = requests.get(CONFUSABLES_URL).text

# parse each line
for line in response.split('\n'):
    line = line.split('#', 1)[0].strip() # ignore everything after the `#` character (comments) and remove whitespaces
    if not len(line): continue # skip empty lines
    
    # format: 123A ;\t456B ;\tMA
    # we want the lines where both the first and the second column have exactly one entry
    src, dst, _ = [x.strip() for x in line.split(';', 2)]
    if len(src.split(' ', 1)) != 1 or len(dst.split(' ', 1)) != 1: continue

    # check that both codepoints are allowed
    src_n = int(src, 16)
    dst_n = int(dst, 16)
    if not is_codepoint_allowed(src_n) or not is_codepoint_allowed(dst_n): continue

    # save the entry
    if confusables.get(dst_n) == None:
        confusables[dst_n] = [src_n]
    else:
        confusables[dst_n].append(src_n)

# print homoglyph function
print('static inline uint homoglyph(uint c)')
print('{')
print('\tswitch(c)')
print('\t{')

for dst_n in sorted(confusables.keys()):
    src_list = confusables.get(dst_n)
    n = 0
    for src_n in src_list:
        n = n + 1
        print(f'\t\tcase {hex(src_n)}:', end='' if n >= len(src_list) else '\n')
    print(f' return {hex(dst_n)};')

print('\t}')
print('\treturn c;')
print('}')