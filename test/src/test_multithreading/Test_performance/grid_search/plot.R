library(ggplot2)
library(dplyr)
library(tidyr)
library(stringr)
raw_text <- paste(readLines("test_grid_search.txt"), collapse = "\n")

# --------------------------------------------------
# Split del testo in linee
lines <- str_split(raw_text, "\n")[[1]]
lines <- lines[nchar(lines) > 0]

# Creazione del dataframe long
df <- do.call(rbind, lapply(lines, function(l) {
  parts <- str_split(str_squish(l), " ")[[1]]
  n_values <- length(parts) - 3
  data.frame(
    Method = rep(parts[1], n_values),
    thread = rep(parts[2], n_values),
    size = rep(parts[3], n_values),
    value = as.numeric(parts[4:length(parts)]),
    stringsAsFactors = FALSE
  )
}))
df <- df[df$value >= 0, ]

# Estrarre il numero di thread come numero
df$thread_n <- as.numeric(str_extract(df$thread, "\\d+"))
df$thread_n[is.na(df$thread_n) | is.nan(df$thread_n)] <- 1
df$Method <- factor(df$Method, levels = c("opt", "opt_parallel", "opt_par_variadic"))
# 3) Plot con boxplot affiancati
p <- ggplot(df, aes(x = factor(thread_n), y = value, fill = Method)) +
  geom_boxplot(position = position_dodge(width = 0.8)) +
  labs(
    x = "Number of threads",
    y = "Time (microseconds)",
    title = "sequential vs gran1 vs gran_input"
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
  scale_fill_brewer(
    palette = "Set2",
    labels = c(
      "opt"   = "Sequential",
      "opt_parallel"        = "Granularity 1",
      "opt_par_variadic"   = "Granularity Input"
    ))
p
###SPEEDUP
library(dplyr)
library(ggplot2)

# Calcolo mediana sequenziale
median_seq <- median(df$value[df$Method == "opt"])

# Calcolo speedup
df_speedup <- df %>%
  filter(Method %in% c("opt_parallel", "opt_par_variadic")) %>%
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
      "opt_parallel" = "#FC8D62",  # salmone
      "opt_par_variadic"       = "#8DA0CB"   # azzurro/verde chiaro
    ),
    labels = c(
      "opt_parallel"     = "granularity 1",
      "opt_par_variadic" = "granularity input"
    )
  )+
  labs(
    x = "Number of threads",
    y = "Speedup",
    title = "Speedup: parallel(gran1) vs parallel(gran_input)",
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
