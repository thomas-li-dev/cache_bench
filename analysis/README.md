# Hit rate analysis

```

Create and activate a Python virtual environment, then install dependencies:

```bash
python3 -m venv venv
source venv/bin/activate
pip install zstandard matplotlib numpy tqdm
```

## Data

Download traces from SIEVE's Twitter KV dataset into `data/`:
https://ftp.pdl.cmu.edu/pub/datasets/twemcacheWorkload/cacheDatasets/twitter/

```bash
mkdir -p data
cd data
for c in 10 26 45 50 53; do
  curl -fLO "https://ftp.pdl.cmu.edu/pub/datasets/twemcacheWorkload/cacheDatasets/twitter/cluster${c}.oracleGeneral.zst"
done
cd ..
```

### 1) Run simulations

```bash
python3 run_simulations.py \
  --trace-dir data/ \
  --trace-globs 'cluster10.oracleGeneral.zst,cluster26.oracleGeneral.zst,cluster45.oracleGeneral.zst,cluster50.oracleGeneral.zst,cluster53.oracleGeneral.zst' \
  --algos fifo,lru \
  --output results/hit_rate.json
```

### 2) Plot Figure from JSON

```bash
python3 plot.py \
  --input results/hit_rate.json \
  --output results/hit_rate.pdf
```