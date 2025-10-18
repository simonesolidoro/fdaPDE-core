# notebook: parse_and_plot_fdaPDE_results.ipynb
import re
import os
from collections import defaultdict
import matplotlib.pyplot as plt

# ottieni la cartella dello script
script_dir = os.path.dirname(os.path.abspath(__file__))
filename = os.path.join(script_dir, "risultati.txt")

# Parsing dati
data = defaultdict(lambda: defaultdict(list))

with open(filename, 'r') as f:
    current_thread = None
    current_method = None
    for line in f:
        line = line.strip()
        if not line:
            continue

        # Trova il numero di thread
        m_thread = re.match(r"=== Numero thread: (\d+) ===", line)
        if m_thread:
            current_thread = int(m_thread.group(1))
            continue

        # Trova il metodo
        m_method = re.match(r"(Sequenziale|Parallel optimize1|Parallel optimize2|Parallel optimize3):", line)
        if m_method:
            current_method = m_method.group(1)
            continue

        # Righe di numeri
        if current_thread is not None and current_method is not None:
            line = line.rstrip(',')
            values = [int(x) for x in line.split(',') if x]
            data[current_thread][current_method].extend(values)

# Creazione delle liste nominate
optimize1 = {n: data[n]['Parallel optimize1'] for n in data}
optimize2 = {n: data[n]['Parallel optimize2'] for n in data}
optimize3 = {n: data[n]['Parallel optimize3'] for n in data}
sequenziale = {n: data[n]['Sequenziale'] for n in data}

# Preparazione dei boxplot
methods = ['Sequenziale', 'Optimize1', 'Optimize2', 'Optimize3']
colors = ['lightblue', 'lightgreen', 'lightcoral', 'khaki']

plt.figure(figsize=(16, 8))

# Per ciascun numero di thread
for idx, n in enumerate(sorted(data.keys()), 1):
    plt.subplot(2, 4, idx)  # 2 righe x 4 colonne
    values = [
        sequenziale[n],
        optimize1[n],
        optimize2[n],
        optimize3[n]
    ]
    plt.boxplot(values, labels=methods, patch_artist=True,
                boxprops=dict(facecolor='lightblue', color='black'),
                medianprops=dict(color='red'))
    plt.title(f"{n} Thread{'s' if n>1 else ''}")
    plt.ylabel("Tempo (microseconds)")
    plt.xticks(rotation=15)

plt.tight_layout()
plt.show()

# BOX PLOT COMBINATO CORRETTO
plt.figure(figsize=(16, 6))

# Raggruppiamo i dati: ogni gruppo è un metodo per un numero di thread
box_data = []
box_labels = []

for n in sorted(data.keys()):
    box_data.append(sequenziale[n])
    box_labels.append(f"{n}T Seq")
    box_data.append(optimize1[n])
    box_labels.append(f"{n}T Opt1")
    box_data.append(optimize2[n])
    box_labels.append(f"{n}T Opt2")
    box_data.append(optimize3[n])
    box_labels.append(f"{n}T Opt3")

plt.boxplot(box_data, labels=box_labels, patch_artist=True,
            boxprops=dict(facecolor='lightblue', color='black'),
            medianprops=dict(color='red'))
plt.xticks(rotation=45)
plt.ylabel("Tempo (microseconds)")
plt.title("Boxplot combinato di tutti i metodi e numeri di thread")
plt.tight_layout()
plt.show()