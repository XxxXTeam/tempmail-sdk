"""
SDK 日志模块
基于 Python 标准库 logging 实现分级日志
默认静默不输出，用户可通过 set_log_level 启用
"""

import logging

LOG_SILENT = 100   # 关闭所有日志
LOG_ERROR = logging.ERROR
LOG_WARNING = logging.WARNING
LOG_INFO = logging.INFO
LOG_DEBUG = logging.DEBUG

# 创建 SDK 专用 logger
_logger = logging.getLogger("tempmail-sdk")
_logger.setLevel(LOG_SILENT)

# 默认不添加 handler，避免重复输出
_handler = logging.StreamHandler()
_handler.setFormatter(logging.Formatter("%(levelname)s %(message)s"))
_logger.addHandler(_handler)
_logger.propagate = False


def set_log_level(level: int) -> None:
    """
    设置日志级别
    默认 LOG_SILENT（不输出任何日志）

    示例:
        from tempmail_sdk import set_log_level, LOG_DEBUG
        set_log_level(LOG_DEBUG)  # 开启所有日志
    """
    _logger.setLevel(level)


def set_logger(logger: logging.Logger) -> None:
    """
    设置自定义 logger 实例
    替换默认的 stderr 输出

    示例:
        import logging
        my_logger = logging.getLogger("my_app")
        set_logger(my_logger)
    """
    global _logger
    _logger = logger


def get_logger() -> logging.Logger:
    """获取当前 SDK 使用的 logger 实例"""
    return _logger
