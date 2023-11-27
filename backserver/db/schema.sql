CREATE DATABASE IF NOT EXISTS `backserver` DEFAULT CHARACTER SET utf8 COLLATE utf8_bin;

DROP TABLE IF EXISTS `backserver`.`backclients`;
CREATE TABLE `backserver`.`backclients` (
  `id` int(20) unsigned NOT NULL AUTO_INCREMENT,
  `botid` varchar(36) NOT NULL,
  `botip` varchar(16) NOT NULL,
  `type` varchar(10) NOT NULL,
  `botport` int(11) NOT NULL,
  `onlinefrom` datetime NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `backid` (`botid`, `type`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

DROP TABLE IF EXISTS `backserver`.`backhistory`;
CREATE TABLE `backserver`.`backhistory` (
  `botid` varchar(36) NOT NULL,
  `type` int(11) NOT NULL,
  `port` int(11) NOT NULL,
  PRIMARY KEY (`botid`, `type`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

CREATE USER IF NOT EXISTS 'backserver'@'localhost' IDENTIFIED BY 'vj6JY1oP7LA9F4276to6y73r88M4h76Er35091K425A3M7U9U69B8H2JE13y';
GRANT USAGE ON *.* TO 'backserver'@'localhost';
GRANT ALL PRIVILEGES ON `backserver`.* TO 'backserver'@'localhost';

CREATE USER IF NOT EXISTS 'backviewer'@'%' IDENTIFIED BY 'VrggeNj1ydwD3Pgt62HrgHXzja8SlJMTSSEBtg2hDxwX6osiMFu43PyjxQvN';
GRANT USAGE ON *.* TO 'backviewer'@'%';
GRANT SELECT ON `backserver`.* TO 'backviewer'@'%';
