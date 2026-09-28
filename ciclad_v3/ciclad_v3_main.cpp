// version 3.0a 2019-09-30 (insert, clean C++ template, stdin/stream)
// version 3.0b 2019-10-04 (beta remove with leaks)
// version 3.0c 2020-07-03 (remove with cleanup function)

#include "ciclad_v3_impl.h"   //uint, ushort, node3, concept3, freenode3()
#include "../utility/usage.h"  //ram-cpu usage utility (Win32 or Linux)
#include <algorithm>  // reverse (SPMF itemset sorting)

#pragma warning(disable : 4996)

using namespace std;

//------------------------------------------------------------------------------
// Idx-inversion itemset export (I/O-only: observation-only serialization of fCI2)
// Safe for add-only (E1) execution — no node.parent chain traversal.
// Invariant under E1: itemset(cid) = { item | cid ∈ idx[item] }
//
// Proof sketch:
//   A. idx[idx[item][k]] = item  (idx is sound)
//   B. No deleted entries persist in idx (no stale IDs under add-only)
//   C. Each (cid, item) pair appears exactly once across all idx vectors (no duplicates)
//   D. Ancestor walk: itemset(cid) = items in idx matching cid (correct for add-only)
//
// For add-only execution, this formula holds because:
//   - idx[item].push_back(cid) is the only mutation adding cid to idx[item]
//   - No path removes a cid from idx[item] without also marking it deleted
//   - The wrapper sends only "add T" (no "del") and then "end", so cleanup() never runs
//------------------------------------------------------------------------------

// Export all non-deleted concepts to SPMF format via idx-inversion.
static void export_fci2_to_spmf(const char *export_spmf,
                                  const vector<vector<uint>> &idx,
                                  const vector<concept3> &fCI2) {
  ofstream spmf_out(export_spmf);
  if (!spmf_out.is_open()) {
    cerr << "Warning: could not open SPMF export file: " << export_spmf << endl;
    return;
  }
  for (size_t cid = 0; cid < fCI2.size(); ++cid) {
    if (fCI2[cid].deleted == 1) continue;
    vector<uint> items;
    if (cid == 0) {
      // Superconcept: itemset is stored directly.
      items = fCI2[cid].itemset;
    } else {
      // Non-root concept: invert the idx to recover the itemset.
      // For every item, scan idx[item] for this cid.
      for (size_t item = 0; item < idx.size(); ++item) {
        for (size_t k = 0; k < idx[item].size(); ++k) {
          if (idx[item][k] == cid) {
            items.push_back((uint)item);
            break;  // each cid appears at most once per idx[item]
          }
        }
      }
      sort(items.begin(), items.end());
    }
    if (items.empty()) continue;
    for (size_t j = 0; j < items.size(); ++j) {
      if (j > 0) spmf_out << ' ';
      spmf_out << (items[j] + 1);  // 0-based → 1-based SPMF
    }
    spmf_out << " #SUP: " << fCI2[cid].supp << "\n";
  }
  spmf_out.close();
}

//------------------------------------------------------------------------------

int main(int argc, char *argv[]) {
#ifdef LINUX
  struct rusage ru;
#endif
  short verbose = 0; short END = 0;
  const char *export_spmf = nullptr;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--export-spmf") == 0 && i + 1 < argc) {
      export_spmf = argv[i + 1];
      ++i;
    }
    else if (strcmp(argv[i], "-v") == 0) {
      verbose = 1;
    }
  }
  clock_t start = clock(); clock_t running = clock();
  std::vector<vector<uint>> idx(10001); //Initalisation de index invers�
  for (int i = 0; i < 10001; ++i) {
    vector<uint> vc; //Reservation
    idx[i] = vc; //Affection
  }
  std::queue<node3 *> tn; // terminal nodes
  tlx::btree_map<uint, node3 *> _rootChild; //stx is the fastest
  std::vector<uint> tmp;
  concept3 superconcept(0, 0, /*0,*/ 0, tmp);
  uint gCid = 0;

  std::vector<concept3> fCI2(1, superconcept); //add one space with one value (CI).
  ++gCid;

  Stats stats = Stats(); // garde les stats

  //node3 **li = (node3 **)malloc(fCI2.size() * sizeof(node3 *));
  //uint allocated_memory = 0; uint allocated_block = 0;
  double ms = 0;
  char s[10000]; char t[4] = { 0 }; char ss[10000];

  printf("Initialisation en %0.4f ms\n", (clock() - start) / (double)CLOCKS_PER_SEC * 1000);

  while ((fgets(s, 10000, stdin) != NULL) && (END == 0)) {
    strncpy(t, s, 3);

    if (strcmp(t, "add") == 0) {
      strcpy(ss, s + 4);
      ms = add(ss, tn, idx, _rootChild, fCI2, &gCid);
#ifdef DEBUG
      stats.insert(&ms);
#endif
    }
    else if (strcmp(t, "del") == 0) {
      strcpy(ss, s + 4);
      ms = del(ss, tn, idx, _rootChild, fCI2, &gCid);
#ifdef DEBUG
      stats.remove(&ms);
#endif
      ms = cleanup(idx, fCI2);
    }
    else if (strcmp(t, "end") == 0) END = 1;
    ++stats.rows_processed;
    if (verbose) {
      if ((stats.rows_processed % 1000 == 0 && stats.rows_processed < 10001) || stats.rows_processed % 10000 == 0) {
        printf("time between checkpoint %0.2f ms, ", (clock() - running) / (double)CLOCKS_PER_SEC * 1000);
        running = clock();
        std::cout << stats.rows_processed << " rows processed, # CI:" << fCI2.size() << endl;
      }
    }
  }

  printf("Stream completed in %0.2f sec, ", (clock() - start) / (double)CLOCKS_PER_SEC);
  std::cout << stats.rows_inserted << " rows inserted, " << stats.rows_removed << " rows removed , idx size/capacity:" << idx.size() << "/" << idx.capacity() << ", # concept:" << fCI2.size() << endl;
  if (verbose) {
    uint nb[11] = { 0,0,0,0,0,0,0,0,0,0,0 };
    for (uint n = 0; n < fCI2.size(); ++n) {
      uint y = fCI2.at(n).supp;
      if (y < 10) {
        ++nb[y];
      }
      else {
        ++nb[10];
      }
    }
    for (uint n = 0; n < 11; ++n) { std::cout << n << "->" << nb[n] << endl; }
  }

  // SPMF export via idx-inversion (safe for add-only E1 protocol)
  if (export_spmf != nullptr) {
    std::cerr << "[DEBUG] export_spmf is set: " << export_spmf << std::endl;
    std::cerr << "[DEBUG] fCI2.size() = " << fCI2.size() << ", idx.size() = " << idx.size() << std::endl;
    export_fci2_to_spmf(export_spmf, idx, fCI2);
  } else {
    std::cerr << "[DEBUG] export_spmf is NULL" << std::endl;
  }

  fCI2.clear();
#ifdef _WIN32
  pu_ram();
#endif
#ifdef LINUX
  gu_all(&ru);
  pu_ram(ru);
#endif
  return EXIT_SUCCESS;
}
