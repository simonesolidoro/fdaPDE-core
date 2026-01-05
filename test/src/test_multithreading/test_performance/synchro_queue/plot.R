library(ggplot2)
library(dplyr)
library(tidyr)
library(stringr)

raw_text <- paste(readLines("miopc_OptO3_runs20_nel160000.txt"), collapse = "\n")

lines <- str_split(raw_text, "\n")[[1]]
lines <- lines[nchar(lines) > 0]

# Creazione del dataframe 
df <- do.call(rbind, lapply(lines, function(l) {
  parts <- str_split(str_squish(l), " ")[[1]]
  n_values <- length(parts) - 3
  data.frame(
    tipo = rep(parts[1], n_values),
    operazione = rep(parts[2], n_values),
    thread = rep(parts[3], n_values),
    value = as.numeric(parts[4:length(parts)]),
    stringsAsFactors = FALSE
  )
}))
df <- df[df$value >= 0, ]

df$thread_n <- as.numeric(str_extract(df$thread, "\\d+"))

plot_single_op <- function(df, op_name) {
  d <- df[df$operazione == op_name & df$thread == "single", ]
  d$tipo <- factor(d$tipo, levels = c("deque", "blocking", "deferred", "relaxed"))
  ggplot(d, aes(x = tipo, y = value, fill = tipo)) +
    geom_boxplot() +
    labs(
      x = "Queue",
      y = "Time (microseconds)",
      title = paste(op_name, "single thread")
    ) +
    scale_y_log10()+
    theme_minimal() +
    theme(
      legend.position = "none",
      axis.title = element_text(size = 40),
      axis.text = element_text(size = 40),
      plot.title = element_text(size = 42, face = "bold")
    )
}

p_push_front  <- plot_single_op(df, "Push_Front")
p_pop_front   <- plot_single_op(df, "Pop_Front")
p_push_back   <- plot_single_op(df, "Push_Back")
p_pop_back    <- plot_single_op(df, "Pop_Back")
p_push_front  
p_pop_front
p_push_back   
p_pop_back    


##### plot multi
plot_multi_op_with_single <- function(df, op_name) {  
  d <- df[df$operazione == op_name, ]
  d$tipo <- factor(d$tipo, levels = c("deque", "blocking", "deferred", "relaxed"))
  d$thread_label <- ifelse(d$thread == "single", "single", as.character(d$thread_n))
  non_single <- unique(d$thread_label[d$thread_label != "single"])
  non_single_num <- sort(na.omit(as.numeric(non_single)))
  non_single_labels <- as.character(non_single_num)
  levels_order <- c("single", non_single_labels)
  d$thread_label <- factor(d$thread_label, levels = levels_order)
  if (length(levels(d$thread_label)) == 0) d$thread_label <- factor(d$thread_label, levels = "single")
  ggplot(d, aes(x = thread_label, y = value, fill = tipo)) +
    geom_boxplot(position = position_dodge(width = 0.8)) +
    labs(
      x = "n_thread",
      y = "Time (microseconds)",
      title = paste(op_name, "multi-threads"),
      fill = NULL
    ) +
    scale_y_log10()+
    theme_minimal() +
    theme(
      axis.title = element_text(size = 40),
      axis.text = element_text(size = 40),
      plot.title = element_text(size = 42, face = "bold"),
      legend.position = c(0.01, 0.99),
      legend.justification = c("left", "top"),
      legend.background = element_rect(fill = "white", color = "black"),
      legend.title = element_blank(),
      legend.text  = element_text(size = 40)
    )
}

p_push_front_all <- plot_multi_op_with_single(df, "Push_Front")
p_push_back_all  <- plot_multi_op_with_single(df, "Push_Back")
p_pop_front_all  <- plot_multi_op_with_single(df, "Pop_Front")
p_pop_back_all   <- plot_multi_op_with_single(df, "Pop_Back")
p_random   <- plot_multi_op_with_single(df, "random")

p_push_front_all
p_push_back_all
p_pop_front_all
p_pop_back_all
p_random


##### MEDIANE PRINT ###################################
######################################################
print_median_single <- function(df) {
  df_single <- df[df$thread == "single", ]  
  stats <- df_single %>%
    group_by(operazione, tipo) %>%
    summarise(
      mediana = median(value),
      n = n(),
      .groups = "drop"
    )
  print(stats, n = Inf)
}

print_median_multi <- function(df) {
  stats <- df %>%
    mutate(thread_label = ifelse(thread == "single", "single", as.character(thread_n))) %>%
    group_by(operazione, tipo, thread_label) %>%
    summarise(
      mediana = median(value),
      n = n(),
      .groups = "drop"
    ) %>%
    arrange(operazione, tipo, as.numeric(thread_label))
  print(stats, n = Inf)
}

print_median_single(df)
print_median_multi(df)

