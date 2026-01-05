
library(ggplot2)
library(dplyr)
library(tidyr)
library(stringr)

raw_text <- paste(readLines("test_assemble.txt"), collapse = "\n")

# --------------------------------------------------
# Split del testo in linee
lines <- str_split(raw_text, "\n")[[1]]
lines <- lines[nchar(lines) > 0]

# Creazione del dataframe long
df <- do.call(rbind, lapply(lines, function(l) {
  parts <- str_split(str_squish(l), " ")[[1]]
  n_values <- length(parts) - 4
  data.frame(
    Method = rep(parts[1], n_values),
    nodi = rep(parts[2], n_values),
    thread = rep(parts[3], n_values),
    size = rep(parts[4], n_values),
    value = as.numeric(parts[5:length(parts)]),
    stringsAsFactors = FALSE
  )
}))
df <- df[df$value >= 0, ]

# Estrarre il numero di thread come numero
df$thread_n <- as.numeric(str_extract(df$thread, "\\d+"))
df$thread_n[is.na(df$thread_n) | is.nan(df$thread_n)] <- 1
df$Method <- factor(df$Method, levels = c("calcolo_triple", "assemble_completo"))

df_assemble <- df[df$Method == "assemble_completo",]
df_calcolotriple <- df[df$Method == "calcolo_triple",]

df_assemble <- df_assemble %>%
  mutate(
    thread = str_trim(as.character(thread)),
    Method = ifelse(thread == "sequenziale", "sequential", "parallel"),
    Method = factor(Method, levels = c("sequential", "parallel"))
  )
df_calcolotriple <- df_calcolotriple %>%
  mutate(
    thread = str_trim(as.character(thread)),
    Method = ifelse(thread == "sequenziale", "sequential", "parallel"),
    Method = factor(Method, levels = c("sequential", "parallel"))
  )
# ===========================
# 3) Plot con boxplot affiancati
library(ggplot2)

p <- ggplot(df_assemble, aes(x = factor(thread_n), y = value, fill = Method)) +
  geom_boxplot(position = position_dodge(width = 0.9), width = 0.7) + # più separati e box più larghi
  scale_fill_brewer(palette = "Set2") + # palette più distinguibile
  labs(
    x = "Number of threads",
    y = "Time (microseconds)",
    title = "Assemble complete"
  ) +
  scale_y_log10() +  # scala logaritmica sull'asse y
  theme_minimal() +
  theme(
    axis.text.x = element_text(size = 40, angle = 0, hjust = 0.5),
    axis.text.y = element_text(size = 40),
    axis.title = element_text(size = 40),
    legend.title = element_text(size = 40),
    legend.text = element_text(size = 40),
    plot.title = element_text(size = 42, face = "bold"),
    legend.position = c(0.85, 0.85) # dentro il plot, alto a destra
  )
p


###########CALCOLO TRIPLE BOXPLOT

p <- ggplot(df_calcolotriple, aes(x = factor(thread_n), y = value, fill = Method)) +
  geom_boxplot(position = position_dodge(width = 0.9), width = 0.7) + # più separati e box più larghi
  scale_fill_brewer(palette = "Set2") + # palette più distinguibile
  labs(
    x = "Number of threads",
    y = "Time (microseconds)",
    title = "Calculation of triplets"
  ) +
  scale_y_log10() +  # scala logaritmica sull'asse y
  theme_minimal() +
  theme(
    axis.text.x = element_text(size = 40, angle = 0, hjust = 0.5),
    axis.text.y = element_text(size = 40),
    axis.title = element_text(size = 40),
    legend.title = element_text(size = 40),
    legend.text = element_text(size = 40),
    plot.title = element_text(size = 42, face = "bold"),
    legend.position = c(0.85, 0.85) # dentro il plot, alto a destra
  )
p


########PLOT SPEEDUP calcolo triple rispetto thread1####################################

library(dplyr)
library(ggplot2)

# Calcolo delle mediane
median_seq <- median(df_calcolotriple$value[df_calcolotriple$Method == "parallel" & df_calcolotriple$thread_n == 1])
median_parallel <- df_calcolotriple %>%
  filter(Method == "parallel") %>%
  group_by(thread_n) %>%
  summarize(median_time = median(value))

# Calcolo dello speedup
median_parallel <- median_parallel %>%
  mutate(speedup = median_seq / median_time)

# Dataframe per la retta ideale y = x
ideal_df <- data.frame(
  thread_n = median_parallel$thread_n,
  speedup = median_parallel$thread_n
)

# Plot con legenda corretta
p_speedup <- ggplot() +
  geom_line(data = median_parallel,
            aes(x = thread_n, y = speedup, color = "Measured speedup"),
            size = 1) +
  geom_point(data = median_parallel,
             aes(x = thread_n, y = speedup, color = "Measured speedup"),
             size = 3) +
  geom_line(data = ideal_df,
            aes(x = thread_n, y = speedup, color = "Max speedup"),
            linetype = "dashed", size = 1.2) +
  
  scale_color_manual(
    name = "",
    values = c("Measured speedup" = "blue",
               "Max speedup" = "red")
  ) +
  
  scale_x_continuous(trans = "log2", breaks = median_parallel$thread_n) +
  scale_y_continuous(trans = "log2", limits = c(0.9, 32)) +
  
  labs(
    x = "Number of threads",
    y = "Speedup",
    title = "Speedup of calculation of triplets"
  ) +
  
  theme_minimal() +
  theme(
    legend.position = c(0.95, 0.05),
    legend.justification = c("right", "bottom"),
    legend.background = element_rect(fill = alpha('white', 0.5)),
    legend.text = element_text(size = 40),
    axis.text = element_text(size = 40),
    axis.title = element_text(size = 40),
    plot.title = element_text(size = 42, face = "bold")
  )

p_speedup


########PLOT SPEEDUP Assemble  rispetto thread1####################################

library(dplyr)
library(ggplot2)

# Calcolo delle mediane
median_seq <- median(df_assemble$value[df_assemble$Method == "parallel" & df_assemble$thread_n == 1])
median_parallel <- df_assemble %>%
  filter(Method == "parallel") %>%
  group_by(thread_n) %>%
  summarize(median_time = median(value))

# Calcolo dello speedup
median_parallel <- median_parallel %>%
  mutate(speedup = median_seq / median_time)

# Dataframe per la retta ideale y = x
ideal_df <- data.frame(
  thread_n = median_parallel$thread_n,
  speedup = median_parallel$thread_n
)

# Plot con legenda corretta
p_speedup <- ggplot() +
  geom_line(data = median_parallel,
            aes(x = thread_n, y = speedup, color = "Measured speedup"),
            size = 1) +
  geom_point(data = median_parallel,
             aes(x = thread_n, y = speedup, color = "Measured speedup"),
             size = 3) +
  
  scale_color_manual(
    name = "",
    values = c("Measured speedup" = "blue")
  ) +
  
  scale_x_continuous(trans = "log2", breaks = median_parallel$thread_n) +
  scale_y_continuous(trans = "log2", limits = c(0.9, 32)) +
  
  labs(
    x = "Number of threads",
    y = "Speedup",
    title = "Speedup of assemble complete"
  ) +
  
  theme_minimal() +
  theme(
    legend.position = c(0.95, 0.95),
    legend.justification = c("right", "top"),
    legend.background = element_rect(fill = alpha('white', 0.5)),
    legend.text = element_text(size = 40),
    axis.text = element_text(size = 40),
    axis.title = element_text(size = 40),
    plot.title = element_text(size = 42, face = "bold")
  )

p_speedup

