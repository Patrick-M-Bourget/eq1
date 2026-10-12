# The CPU A/B's summary (scripts/check.sh cpu <ref>; docs/performance.md, "CPU A/B"). Reads one line per
# round and scene, tab-separated: the scene's name, its load in this tree (after) and in <ref> (before).
# Prints, per scene in the order first read, the median after/before ratio, its min and max, how many rounds
# went up and down, and a verdict: "slower" or "faster" when the median is at least min_change from 1.0 and
# at least min_agree rounds went that way, "no clear change" otherwise.
# Variables: min_change (a fraction, 0.03 for 3%), min_agree (rounds).

function median(values, n,    i, j, t) {
    for (i = 2; i <= n; i++)
        for (j = i; j > 1 && values[j - 1] > values[j]; j--) {
            t = values[j]; values[j] = values[j - 1]; values[j - 1] = t
        }
    return n % 2 ? values[(n + 1) / 2] : (values[n / 2] + values[n / 2 + 1]) / 2
}

$3 > 0 {
    if (!($1 in rounds)) order[++scenes] = $1
    ratio = $2 / $3
    ratios[$1, ++rounds[$1]] = ratio
    if (ratio > 1) up[$1]++
    if (ratio < 1) down[$1]++
}

END {
    printf "%-38s %7s  %-15s  %-9s  %s\n", "CPU scene", "Median", "Min to max", "Up/down", "Verdict"
    for (s = 1; s <= scenes; s++) {
        name = order[s]
        n = rounds[name]
        lo = hi = ratios[name, 1]
        for (i = 1; i <= n; i++) {
            values[i] = ratios[name, i]
            if (values[i] < lo) lo = values[i]
            if (values[i] > hi) hi = values[i]
        }
        m = median(values, n)
        verdict = "no clear change"
        if (m >= 1 + min_change && up[name] + 0 >= min_agree) verdict = "slower"
        if (m <= 1 - min_change && down[name] + 0 >= min_agree) verdict = "faster"
        printf "%-38s %7.3f  %.3f to %.3f  %d/%d of %-2d  %s\n", name, m, lo, hi, up[name], down[name], n, verdict
    }
}
