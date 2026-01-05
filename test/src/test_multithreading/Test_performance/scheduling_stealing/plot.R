
library(ggplot2)
library(dplyr)
library(tidyr)
library(stringr)


raw_text <- paste(readLines("test_scheduling_stealing_.txt"), collapse = "\n")

# --------------------------------------------------

# Converte il testo in dataframe
lines <- str_split(raw_text, "\n")[[1]]
lines <- lines[nchar(lines) > 0]

df <- do.call(rbind, lapply(lines, function(l) {
  parts <- str_split(str_squish(l), " ")[[1]]
  data.frame(
    scheme = parts[1],
    threads = parts[2],
    value = as.numeric(parts[-c(1,2)]),
    stringsAsFactors = FALSE
  )
}))

# Estrarre il numero di thread come numero
df$thread_n <- as.numeric(str_extract(df$threads, "\\d+"))



# ------ PLOT ------


p <- ggplot(df, aes(
  x = factor(thread_n),
  y = value,
  fill = scheme
)) +
  geom_boxplot(position = position_dodge(width = 0.8)) +
  labs(
    x = "Number of threads",
    y = "Time (microseconds)",
    title = "Scheduling_Stealing"
  ) +
  theme_minimal() +
  theme(
    legend.position = c(0.98, 0.98),
    legend.justification = c("right", "top"),
    legend.background = element_rect(fill = alpha('white', 0.5)),
    legend.title = element_blank(),
    legend.text  = element_text(size = 40),
    axis.title   = element_text(size = 40),
    axis.text    = element_text(size = 40),
    plot.title   = element_text(size = 42, face = "bold"),
    axis.text.x  = element_text(angle = 0, hjust = 0.5)
  )
p
