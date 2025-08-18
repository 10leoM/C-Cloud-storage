class SegmentVideoPlayer {
    constructor(videoElement, statusElement) {
        this.video = videoElement;
        this.statusElement = statusElement;
        this.mediaSource = null;
        this.sourceBuffer = null;
        this.videoUrl = 'your-video.mp4'; // 替换为你的视频URL
        this.sessionId = this.getSessionId(); // 从存储中获取或生成sessionId
        this.totalBytes = 0;
        this.loadedBytes = 0;
        this.chunkSize = 1024 * 1024 * 2; // 2MB chunks
        this.isInitialized = false;
        this.metadataLoaded = false;
        this.requestQueue = [];
        this.isBuffering = false;
        
        this.initPlayer();
    }
    
    getSessionId() {
        // 从cookie或localStorage获取sessionId，如果没有则生成一个新的
        let sessionId = localStorage.getItem('sessionId');
        if (!sessionId) {
            sessionId = 'session_' + Math.random().toString(36).substr(2, 9);
            localStorage.setItem('sessionId', sessionId);
        }
        return sessionId;
    }
    
    initPlayer() {
        this.updateStatus('初始化媒体源...');
        this.mediaSource = new MediaSource();
        this.video.src = URL.createObjectURL(this.mediaSource);
        
        this.mediaSource.addEventListener('sourceopen', () => {
            this.updateStatus('媒体源已打开，准备加载视频...');
            this.sourceBuffer = this.mediaSource.addSourceBuffer('video/mp4; codecs="avc1.42E01E, mp4a.40.2"');
            this.sourceBuffer.addEventListener('updateend', this.onUpdateEnd.bind(this));
            this.sourceBuffer.addEventListener('error', this.onError.bind(this));
            
            // 先加载视频元数据
            this.loadInitialMetadata();
        });
        
        this.mediaSource.addEventListener('sourceended', () => {
            this.updateStatus('媒体源已结束');
        });
        
        this.video.addEventListener('playing', () => {
            this.updateStatus('视频播放中');
            this.checkBuffer();
        });
        
        this.video.addEventListener('waiting', () => {
            this.updateStatus('视频缓冲中...');
            this.checkBuffer();
        });
        
        this.video.addEventListener('progress', () => {
            this.checkBuffer();
        });
    }
    
    async loadInitialMetadata() {
        try {
            this.updateStatus('加载视频元数据...');
            
            // 获取前64KB数据，通常包含moov box
            const response = await this.fetchWithSessionId(this.videoUrl, 'bytes=0-65535');
            
            if (response.status === 206) {
                const contentRange = response.headers.get('Content-Range');
                this.totalBytes = parseInt(contentRange.split('/')[1]);
                
                const data = await response.arrayBuffer();
                this.sourceBuffer.appendBuffer(data);
                
                this.metadataLoaded = true;
                this.loadedBytes = 65536;
                this.updateStatus(`元数据已加载，视频总大小: ${this.formatBytes(this.totalBytes)}`);
                
                // 开始预加载初始视频内容
                this.loadNextSegment();
            } else {
                throw new Error('服务器不支持范围请求');
            }
        } catch (error) {
            this.updateStatus(`加载元数据失败: ${error.message}`);
            console.error('加载元数据失败:', error);
        }
    }
    
    async fetchWithSessionId(url, range = null) {
        const headers = {
            'X-Session-ID': this.sessionId
        };
        
        if (range) {
            headers['Range'] = range;
        }
        
        return fetch(url, {
            headers: headers
        });
    }
    
    onUpdateEnd() {
        this.isBuffering = false;
        
        if (!this.sourceBuffer.updating && this.mediaSource.readyState === 'open') {
            if (this.loadedBytes < this.totalBytes) {
                this.loadNextSegment();
            } else {
                this.updateStatus('视频加载完成');
                this.mediaSource.endOfStream();
            }
        }
    }
    
    onError(error) {
        this.updateStatus(`发生错误: ${error.message}`);
        console.error('SourceBuffer错误:', error);
    }
    
    async loadNextSegment() {
        if (this.isBuffering || !this.metadataLoaded || this.sourceBuffer.updating) {
            return;
        }
        
        // 计算下一个要加载的片段
        const segmentStart = this.loadedBytes;
        const segmentEnd = Math.min(this.loadedBytes + this.chunkSize - 1, this.totalBytes - 1);
        
        // 如果已经加载到末尾，不再请求
        if (segmentStart >= this.totalBytes) {
            return;
        }
        
        this.isBuffering = true;
        this.updateStatus(`加载视频片段: ${this.formatBytes(segmentStart)}-${this.formatBytes(segmentEnd)}`);
        
        try {
            const response = await this.fetchWithSessionId(
                this.videoUrl, 
                `bytes=${segmentStart}-${segmentEnd}`
            );
            
            if (response.status === 206) {
                const data = await response.arrayBuffer();
                this.sourceBuffer.appendBuffer(data);
                this.loadedBytes = segmentEnd + 1;
                
                const percentLoaded = Math.round((this.loadedBytes / this.totalBytes) * 100);
                this.updateStatus(`已加载: ${percentLoaded}% (${this.formatBytes(this.loadedBytes)}/${this.formatBytes(this.totalBytes)})`);
            } else {
                throw new Error('片段请求失败');
            }
        } catch (error) {
            this.isBuffering = false;
            this.updateStatus(`加载片段失败: ${error.message}`);
            console.error('加载片段失败:', error);
        }
    }
    
    checkBuffer() {
        // 检查当前播放位置和缓冲情况，决定是否需要加载更多数据
        if (!this.video.buffered.length) return;
        
        const currentTime = this.video.currentTime;
        const bufferedEnd = this.video.buffered.end(this.video.buffered.length - 1);
        const bufferThreshold = 10; // 提前10秒加载
        
        // 如果缓冲即将用完，加载更多数据
        if (bufferedEnd - currentTime < bufferThreshold && !this.isBuffering) {
            this.loadNextSegment();
        }
    }
    
    updateStatus(message) {
        this.statusElement.textContent = message;
        console.log(message);
    }
    
    formatBytes(bytes) {
        if (bytes === 0) return '0 Bytes';
        const k = 1024;
        const sizes = ['Bytes', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }
}

// 初始化播放器
document.addEventListener('DOMContentLoaded', () => {
    const videoPlayer = new SegmentVideoPlayer(
        document.getElementById('videoPlayer'),
        document.getElementById('loadingStatus')
    );
});