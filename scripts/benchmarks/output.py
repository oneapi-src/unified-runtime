# Copyright (C) 2024 Intel Corporation
# Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
# See LICENSE.TXT
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

import collections
from benches.base import Result
import math
import matplotlib.pyplot as plt

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
<details>
<summary>{bname}</summary>

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

</details>
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

def generate_summary_table_and_chart(chart_data: dict[str, list[Result]]):
    summary_table = "| Benchmark | " + " | ".join(chart_data.keys()) + " |\n"
    summary_table += "|---" * (len(chart_data) + 1) + "|\n"

    # Collect all benchmarks and their results
    benchmark_results = collections.defaultdict(dict)
    for key, results in chart_data.items():
        for res in results:
            benchmark_results[res.name][key] = res

    # Generate the table rows
    diffs_dict = {}

    for bname, results in benchmark_results.items():
        row = f"| {bname} |"
        best_value = None
        best_key = None

        # Determine the best value
        for key, res in results.items():
            if best_value is None or (res.lower_is_better and res.value < best_value) or (not res.lower_is_better and res.value > best_value):
                best_value = res.value
                best_key = key

        # Generate the row with the best value highlighted
        for key in chart_data.keys():
            if key in results:
                value = results[key].value
                if key == best_key:
                    row += f" <ins>{value}</ins> |"  # Highlight the best value
                else:
                    row += f" {value} |"
            else:
                row += " - |"

        if len(chart_data.keys()) == 2:
            key0 = list(chart_data.keys())[0]
            key1 = list(chart_data.keys())[1]
            if (key0 in results) and (key1 in results):
                v0 = results[key0].value;
                v1 = results[key1].value;
                diff = None
                if v0 != 0 and results[key0].lower_is_better:
                    diff = v1/v0
                elif v1 != 0 and not results[key0].lower_is_better:
                    diff = v0/v1
                if diff != None:
                    row += f"diff:{(diff * 100):.1f}%"
                    diffs_dict[results[key0].label] = diff

        summary_table += row + "\n"

    n = len(diffs_dict)
    values = list(diffs_dict.values())


    if n > 0: # we have some values to compare
        # calculate geom. mean
        product = 1
        for value in values:
            product *= value

        geometric_mean = product ** (1/n)
        summary_table += f"Mean performance: {(geometric_mean*100):.1f}% of baseline (higher is better)" + "\n"
        print(f"Mean performance: {(geometric_mean*100):.1f}% of baseline")

        # create hiriz. bar chart for particular benchmarks

        labels = list(diffs_dict.keys())
        values = list(diffs_dict.values())

        colors = ['blue' if value > 1 else 'red' for value in values]

        # Adding labels next to each bar
        for index, value in enumerate(values):
            plt.text(value/2, index, f"{(value*100):.2f}%", va='center', color='white')

        plt.figure(figsize=(8, len(values) * .5))

        # Create horizontal bar chart
        plt.barh(labels, values, color=colors)

        # Adding labels and title
        plt.xlabel('Speedup')
        plt.title('Performance compared to baseline')

        # Save the plot as a PNG file
        plt.savefig('benchmarks_chart.png', bbox_inches='tight')


    return summary_table

def generate_markdown(chart_data: dict[str, list[Result]]):
    mermaid_script = generate_mermaid_script(chart_data)
    summary_table = generate_summary_table_and_chart(chart_data)

    return f"""
# Summary
<ins>result</ins> is better\n
{summary_table}
# Charts
{mermaid_script}
# Details
{generate_markdown_details(chart_data["This PR"])}
"""
