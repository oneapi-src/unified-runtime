# Copyright (C) 2024 Intel Corporation
# Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
# See LICENSE.TXT
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

import collections
from benches.base import Result
import math

# Function to generate the mermaid bar chart script
def generate_mermaid_script(chart_data: dict[str, list[Result]]):
    benches = collections.defaultdict(list)
    for (_, data) in chart_data.items():
        for res in data:
            benches[res.name].append(res.label)

    mermaid_script = ""

    for (bname, labels) in benches.items():
        # remove duplicates
        labels = list(dict.fromkeys(labels))
        mermaid_script += f"""
```mermaid
---
config:
    gantt:
        rightPadding: 10
        leftPadding: 120
        sectionFontSize: 10
        numberSectionStyles: 2
---
gantt
    title {bname}
    todayMarker off
    dateFormat  X
    axisFormat %s
"""
        for label in labels:
            nbars = 0
            print_label = label.replace(" ", "<br>")
            mermaid_script += f"""
    section {print_label}
"""
            for (name, data) in chart_data.items():
                for res in data:
                    if bname == res.name and label == res.label:
                        nbars += 1
                        mean = res.value
                        crit = "crit," if name == "This PR" else ""
                        mermaid_script += f"""
        {name} ({mean} {res.unit})   : {crit} 0, {int(mean)}
"""
            padding = 4 - nbars
            if padding > 0:
                for _ in range(padding):
                    mermaid_script += f"""
    -   : 0, 0
"""
        mermaid_script += f"""
```
"""

    return mermaid_script

# Function to generate the markdown collapsible sections for each variant
def generate_markdown_details(results: list[Result]):
    markdown_sections = []
    for res in results:
        env_vars_str = '\n'.join(f"{key}={value}" for key, value in res.env.items())
        markdown_sections.append(f"""
<details>
<summary>{res.label}</summary>

#### Environment Variables:
{env_vars_str}

#### Command:
{' '.join(res.command)}

#### Output:
{res.stdout}

</details>
""")
    return "\n".join(markdown_sections)

def generate_summary(chart_data: dict[str, list[Result]]) -> str:
    # Calculate the geometric mean value of "This PR" for each benchmark
    this_pr_geomeans = {}
    for res in chart_data["This PR"]:
        if res.name not in this_pr_geomeans:
            this_pr_geomeans[res.name] = []
        this_pr_geomeans[res.name].append(res.value)
    for bname in this_pr_geomeans:
        product = math.prod(this_pr_geomeans[bname])
        this_pr_geomeans[bname] = product ** (1 / len(this_pr_geomeans[bname]))

    # Calculate the percentage for each entry relative to "This PR" using geometric mean
    summary_data = {"This PR": 100}
    for entry_name, results in chart_data.items():
        if entry_name == "This PR":
            continue
        entry_product = math.prod([res.value for res in results if res.name in this_pr_geomeans])
        entry_geomean = entry_product ** (1 / len(results)) if results else 0
        if entry_geomean and this_pr_geomeans.get(results[0].name):
            percentage = (entry_geomean / this_pr_geomeans[results[0].name]) * 100
        else:
            percentage = 0
        summary_data[entry_name] = percentage

    markdown_table = "| Name | Result % |\n| --- | --- |\n"
    for entry_name, percentage in summary_data.items():
        markdown_table += f"| {entry_name} | {percentage:.2f}% |\n"

    return markdown_table

def generate_markdown(chart_data: dict[str, list[Result]]):
    mermaid_script = generate_mermaid_script(chart_data)

    return f"""
# Summary
{generate_summary(chart_data)}
# Benchmark Results
{mermaid_script}
## Details
{generate_markdown_details(chart_data["This PR"])}
"""
