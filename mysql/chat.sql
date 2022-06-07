/*好友关系表*/
CREATE TABLE `friend` (
  `name1` varchar(20) DEFAULT NULL,
  `name2` varchar(20) DEFAULT NULL,
  `status` smallint(6) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;


/*群聊历史记录表*/
CREATE TABLE `group_his` (
  `group_name` varchar(20) DEFAULT NULL,
  `member` varchar(20) DEFAULT NULL,
  `message` varchar(200) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;


/*群聊表*/
CREATE TABLE `groups` (
  `group_name` varchar(20) DEFAULT NULL,
  `member` varchar(20) DEFAULT NULL,
  `status` smallint(6) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

/*历史记录表*/
CREATE TABLE `history` (
  `name1` varchar(20) DEFAULT NULL,
  `name2` varchar(20) DEFAULT NULL,
  `message` varchar(200) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

/*在线用户表*/
CREATE TABLE `online` (
  `username` varchar(20) DEFAULT NULL,
  `socket` smallint(6) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;


/*用户信息表*/
CREATE TABLE `person` (
  `username` varchar(20) DEFAULT NULL,
  `passwd` varchar(20) DEFAULT NULL,
  `question` varchar(20) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

/*离线消息表*/
CREATE TABLE `record` (
  `name1` varchar(20) DEFAULT NULL,
  `name2` varchar(20) DEFAULT NULL,
  `message` varchar(200) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;