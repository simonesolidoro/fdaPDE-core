library(ggplot2)
library(dplyr)
library(tidyr)

raw_text <- paste(readLines("test_sum_matrix.txt"), collapse = "\n")

# --------------------------------------------------

# Converte il testo in dataframe
lines <- str_split(raw_text, "\n")[[1]]
lines <- lines[nchar(lines) > 0]

df <- do.call(rbind, lapply(lines, function(l) {
  parts <- str_split(str_squish(l), " ")[[1]]
  data.frame(
    Method = parts[1],
    threads = parts[2],
    value = as.numeric(parts[-c(1,2)]),
    stringsAsFactors = FALSE
  )
}))


df$thread_n <- as.numeric(str_extract(df$threads, "\\d+"))
df$Method <- factor(df$Method, levels = c("For", "Parallel_for", "OpenMp"))

# ===========================
# 3) Plot con boxplot affiancati
p <- ggplot(df, aes(x = factor(threads), y = value, fill = Method)) +
  geom_boxplot(position = position_dodge(width = 0.8)) +
  labs(
    x = "Number of threads",
    y = "Time (microseconds)",
    title = "for vs parallel_for vs OpenMP"
  ) +
  scale_y_log10() +  # scala logaritmica sull'asse y
  theme_minimal() +
  theme(
    legend.position = c(0.95, 0.95),
    legend.justification = c("right", "top"),
    legend.background = element_rect(fill = alpha('white', 0.5)),
    legend.title = element_text(size = 40),
    legend.text  = element_text(size = 40),
    plot.title   = element_text(size = 42, face = "bold"),  # titolo più grande
    axis.title   = element_text(size = 40),                 # etichette assi più grandi
    axis.text    = element_text(size = 40)                 # numeri assi più grandi
  ) +
  scale_fill_brewer(palette = "Set2")
p


###SPEEDUP
library(dplyr)
library(ggplot2)

# Calcolo mediana sequenziale
median_seq <- median(df$value[df$Method == "For"])

# Calcolo speedup
df_speedup <- df %>%
  filter(Method %in% c("Parallel_for", "OpenMp")) %>%
  group_by(Method, thread_n) %>%
  summarize(median_time = median(value), .groups = "drop") %>%
  mutate(speedup = median_seq / median_time)

# Plot speedup log2-log2 con colori della palette Set2
p_speedup <- ggplot(df_speedup, aes(x = thread_n, y = speedup, color = Method)) +
  geom_point(size = 3) +
  geom_line(size = 1) +
  geom_abline(intercept = 0, slope = 1, linetype = "dashed", color = "red") + # speedup ideale
  scale_x_continuous(trans = "log2", breaks = df_speedup$thread_n) +
  scale_y_continuous(trans = "log2") +
  scale_color_manual(
    values = c(
      "Parallel_for" = "#FC8D62",  # salmone
      "OpenMp"       = "#8DA0CB"   # azzurro/verde chiaro
    )
  )+
  labs(
    x = "Number of threads",
    y = "Speedup",
    title = "Speedup: parallel_for vs OpenMP",
    color = "Method"
  ) +
  theme_minimal() +
  theme(
    plot.title   = element_text(size = 42, face = "bold"),
    axis.title   = element_text(size = 40),
    axis.text    = element_text(size = 40),
    legend.title = element_text(size = 40),
    legend.text  = element_text(size = 40),
    legend.position = c(0.95, 0.05),
    legend.justification = c("right", "bottom"),
    legend.background = element_rect(fill = alpha('white', 0.5)),
  )

# Mostra il plot
p_speedup
