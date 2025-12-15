#!/usr/bin/env python3
#
# Copyright (C) 2025 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import re
import json
import argparse

def get_param_type(param_declaration):
    """Extracts the type from a parameter declaration like 'const char* name'."""
    param_declaration = param_declaration.strip()
    if not param_declaration:
        return None

    if param_declaration == '...':
        return '...'

    # Find the last word which we assume is the variable name.
    # A simple rsplit is enough.
    parts = param_declaration.rsplit(None, 1)
    if len(parts) == 2:
        # Return the first part, which is the type
        return parts[0]
    else:
        # No variable name found, so the whole string is the type
        return param_declaration

def parse_jvmti_h(file_content):
    """Parses the jvmti.h file content to extract function definitions."""
    functions = []

    # Find the jvmtiInterface_1_ struct content using regex.
    match = re.search(r'typedef struct jvmtiInterface_1_ \{(.*?)\} jvmtiInterface_1;', file_content, re.DOTALL)
    if not match:
        raise ValueError("Could not find the jvmtiInterface_1_ struct definition in the provided file.")

    struct_content = match.group(1)

    # Split the struct content into sections for each function, using the numbered comments as delimiters.
    sections = re.split(r'(/\*\s*\d+\s*:.*?\*/)', struct_content)

    # The result of the split is [garbage, comment1, code1, comment2, code2, ...].
    # We iterate over the comment-code pairs.
    i = 1
    while i < len(sections):
        comment_part = sections[i]
        code_part = sections[i+1]
        i += 2

        index_match = re.search(r'/\*\s*(\d+)\s*:', comment_part)
        if not index_match:
            continue

        index = int(index_match.group(1))

        # Normalize whitespace in the code part.
        code = re.sub(r'\s+', ' ', code_part.strip())

        if 'RESERVED' in comment_part.upper():
            continue

        # Regex to parse the function pointer definition.
        func_match = re.match(r'(.+?)\s*\(\s*JNICALL\s*\*\s*(\w+)\s*\)\s*\((.*)\)\s*;', code)
        if func_match:
            return_type = func_match.group(1).strip()
            name = func_match.group(2).strip()
            params_str = func_match.group(3).strip()

            param_types = []
            if params_str:
                param_declarations = [p.strip() for p in params_str.split(',')]
                for decl in param_declarations:
                    param_type = get_param_type(decl)
                    if param_type:
                        param_types.append(param_type)

            functions.append({
                "index": index,
                "name": name,
                "param_types": param_types,
                "return_type": return_type
            })

    return {"interface": "jvmtiEnv", "functions": functions}

def main():
    """Main function to parse arguments and run the script."""
    parser = argparse.ArgumentParser(description='Parse jvmti.h and generate a JSON API file.')
    parser.add_argument('input_file', help='Path to the jvmti.h file')
    parser.add_argument('output_file', help='Path to the output JSON file')
    args = parser.parse_args()

    try:
        with open(args.input_file, 'r') as f:
            content = f.read()

        api_data = parse_jvmti_h(content)

        with open(args.output_file, 'w') as f:
            json.dump(api_data, f, indent=2)

        print(f"Successfully generated JSON API file at: {args.output_file}")

    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == '__main__':
    main()

