import os

def get_all_files():
    directories = ["core", "infrastructure", "apps/local_vpl"]
    headers = []
    sources = []
    
    for root_dir in directories:
        for root, _, files in os.walk(root_dir):
            for file in files:
                full_path = os.path.join(root, file).replace("\\", "/")
                if file.endswith(".hpp") or file.endswith(".h"):
                    headers.append(full_path)
                elif file.endswith(".cpp") and not file.endswith("main.cpp"):
                    sources.append(full_path)
    
    # הגדרת סדר עדיפויות לקובצי כותרת בסיסיים כדי למנוע שגיאות "does not name a type"
    def header_priority(path):
        if "Position.hpp" in path:
            return 0
        if "Piece.hpp" in path:
            return 1
        if "Board.hpp" in path:
            return 2
        return 3

    # מיון קובצי ה-hpp לפי סדר העדיפויות שקבענו
    headers.sort(key=header_priority)
                    
    # חיבור כל החלקים כשה-main תמיד בסוף
    return headers + sources + ["apps/local_vpl/main.cpp"]

files_to_bundle = get_all_files()
output_file = "submission.cpp"

with open(output_file, "w", encoding="utf-8") as outfile:
    # כתיבת כל ספריות המערכת הנדרשות בראש הקובץ המאוחד
    outfile.write("#include <iostream>\n#include <vector>\n#include <string>\n#include <unordered_set>\n#include <sstream>\n#include <string_view>\n#include <cmath>\n#include <algorithm>\n\n")
    
    for file_path in files_to_bundle:
        if os.path.exists(file_path):
            with open(file_path, "r", encoding="utf-8") as infile:
                lines = infile.readlines()
                for line in lines:
                    # השמטת includes מקומיים והגנות כותרת لمنע כפילויות שווא
                    if '#include "' in line or "#ifndef" in line or "#define" in line or "#endif" in line:
                        continue
                    outfile.write(line)
            outfile.write("\n\n")

print(f"הקובץ {output_file} נוצר בהצלחה בצורה אוטומטית ומאורגנת!")