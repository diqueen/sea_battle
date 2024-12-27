class Game {
    constructor() {
        this.playerBoard = document.getElementById('player-board');
        this.enemyBoard = document.getElementById('enemy-board');
        this.commandInput = document.getElementById('command-input');
        this.terminalOutput = document.getElementById('terminal-output');
        this.gameActive = false;
        this.isGameOver = false;
        this.width = 10;
        this.height = 10;
        
        this.createBoards();
        this.setupEventListeners();
        this.initializeGame();
    }

    async initializeGame() {
        try {
            const response = await fetch('/game-state');
            if (response.ok) {
                const data = await response.json();
                this.gameActive = true;
                this.updateBoard(this.playerBoard, data.myBoard, true);
                this.updateBoard(this.enemyBoard, data.enemyBoard, false);
                this.startUpdateInterval();
            }
        } catch (error) {
            console.error('Error initializing game:', error);
        }
    }

    startUpdateInterval() {
        if (this.updateInterval) {
            clearInterval(this.updateInterval);
        }
        this.updateInterval = setInterval(() => this.updateGameState(), 1000);
    }

    stopUpdateInterval() {
        if (this.updateInterval) {
            clearInterval(this.updateInterval);
            this.updateInterval = null;
        }
    }

    async updateGameState() {
        if (this.isGameOver) return;

        try {
            const response = await fetch('/game-state');
            if (!response.ok) {
                throw new Error('Network response was not ok');
            }

            const data = await response.json();
            
            this.updateBoard(this.playerBoard, data.myBoard, true);
            this.updateBoard(this.enemyBoard, data.enemyBoard, false);

            if (data.gameOver && !this.isGameOver) {
                await new Promise(resolve => setTimeout(resolve, 1000));

                this.isGameOver = true;
                this.gameActive = false;

                const gameResult = document.getElementById('gameResult');
                if (data.winner === 'player') {
                    gameResult.textContent = 'You Won!';
                    gameResult.className = 'game-result win';
                    this.appendToConsole('Game Over - You Won!');
                } else {
                    gameResult.textContent = 'You Lost!';
                    gameResult.className = 'game-result lose';
                    this.appendToConsole('Game Over - You Lost!');
                }

                this.stopUpdateInterval();
            }
        } catch (error) {
            console.error('Error updating game state:', error);
            this.appendToConsole(`Error updating game state: ${error.message}`);
        }
    }

    async makeShot(x, y) {
        if (this.isGameOver || !this.gameActive) return;

        try {
            const shotCommand = `shot ${x} ${y}`;
            this.appendToConsole(`> ${shotCommand}`);

            const response = await fetch('/command', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ command: shotCommand }),
            });

            if (!response.ok) {
                throw new Error('Network response was not ok');
            }

            const result = await response.json();
            
            if (result.response) {
                this.appendToConsole(result.response);
            }

            await new Promise(resolve => setTimeout(resolve, 300));
            
            const stateResponse = await fetch('/game-state');
            if (!stateResponse.ok) {
                throw new Error('Network response was not ok');
            }

            const gameState = await stateResponse.json();

            this.updateBoard(this.playerBoard, gameState.myBoard, true);
            this.updateBoard(this.enemyBoard, gameState.enemyBoard, false);

            if (gameState.gameOver && !this.isGameOver) {
                await new Promise(resolve => setTimeout(resolve, 1000));

                this.isGameOver = true;
                this.gameActive = false;

                const gameResult = document.getElementById('gameResult');
                if (gameState.winner === 'player') {
                    gameResult.textContent = 'You Won!';
                    gameResult.className = 'game-result win';
                    this.appendToConsole('Game Over - You Won!');
                } else {
                    gameResult.textContent = 'You Lost!';
                    gameResult.className = 'game-result lose';
                    this.appendToConsole('Game Over - You Lost!');
                }

                this.stopUpdateInterval();
            }
        } catch (error) {
            console.error('Error making shot:', error);
            this.appendToConsole(`Error: ${error.message}`);
        }
    }

    startUpdateInterval() {
        this.stopUpdateInterval();
        this.updateInterval = setInterval(() => {
            if (!this.isGameOver) {
                this.updateGameState();
            }
        }, 1000);
    }

    stopUpdateInterval() {
        if (this.updateInterval) {
            clearInterval(this.updateInterval);
            this.updateInterval = null;
        }
    }

    async handleCommand(command) {
        try {
            const response = await fetch('/command', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ command }),
            });

            if (!response.ok) {
                throw new Error('Network response was not ok');
            }

            const result = await response.json();
            
            if (result.response) {
                this.appendToConsole(result.response);
            }

            if (command === 'start') {
                this.gameActive = true;
                this.startUpdateInterval();
            } else if (command === 'stop') {
                this.gameActive = false;
                this.stopUpdateInterval();
            }

            await this.updateGameState();
            
            return result;
        } catch (error) {
            console.error('Error executing command:', error);
            this.appendToConsole(`Error: ${error.message}`);
            return null;
        }
    }

    setupEventListeners() {
        this.commandInput.addEventListener('keypress', async (event) => {
            if (event.key === 'Enter') {
                const command = this.commandInput.value.trim();
                if (command) {
                    this.appendToConsole(`> ${command}`);
                    await this.handleCommand(command);
                    this.commandInput.value = '';
                }
            }
        });
    }

    createBoards() {
        this.playerBoard.innerHTML = '';
        this.enemyBoard.innerHTML = '';
        
        const createGrid = (isEnemy) => {
            const grid = document.createElement('div');
            grid.className = 'grid';
            
            const corner = document.createElement('div');
            corner.className = 'coordinate-label corner';
            grid.appendChild(corner);
            
            for (let x = 0; x < this.width; x++) {
                const label = document.createElement('div');
                label.className = 'coordinate-label';
                label.textContent = x;
                grid.appendChild(label);
            }
            
            for (let y = 0; y < this.height; y++) {
                const rowLabel = document.createElement('div');
                rowLabel.className = 'coordinate-label';
                rowLabel.textContent = y;
                grid.appendChild(rowLabel);
                
                for (let x = 0; x < this.width; x++) {
                    const cell = document.createElement('div');
                    cell.className = 'cell';
                    cell.dataset.x = x;
                    cell.dataset.y = y;
                    grid.appendChild(cell);
                }
            }
            
            return grid;
        };
        
        const playerGrid = createGrid(false);
        const enemyGrid = createGrid(true);
        
        this.playerBoard.appendChild(playerGrid);
        this.enemyBoard.appendChild(enemyGrid);

        this.enemyBoard.addEventListener('click', async (event) => {
            if (this.isGameOver || !this.gameActive) return;

            const cell = event.target.closest('.cell');
            if (!cell) return;

            const x = parseInt(cell.dataset.x);
            const y = parseInt(cell.dataset.y);

            await this.makeShot(x, y);
        });
    }

    clearBoards() {
        document.querySelectorAll('.cell').forEach(cell => {
            cell.classList.remove('ship', 'hit', 'miss', 'kill', 'surrounding');
        });
    }

    updateBoard(board, state, isPlayerBoard) {
        state.forEach((row, y) => {
            row.forEach((cell, x) => {
                const cellElement = board.querySelector(`.grid .cell[data-x="${x}"][data-y="${y}"]`);
                if (cellElement) {
                    cellElement.classList.remove('ship', 'hit', 'miss', 'destroyed', 'surrounding');
                    
                    if (cell === 1 && isPlayerBoard) { // SHIP
                        cellElement.classList.add('ship');
                    } else if (cell === 2) { // HIT
                        cellElement.classList.add('hit');
                    } else if (cell === 3) { // MISS
                        cellElement.classList.add('miss');
                    } else if (cell === 4) { // DESTROYED
                        cellElement.classList.add('destroyed');
                    } else if (cell === 5) { // SURROUNDING
                        cellElement.classList.add('surrounding');
                    }
                }
            });
        });
    }

    async sendCommand(command) {
        console.log('Sending command:', command);
        try {
            const response = await fetch('/command', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ command: command })
            });

            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }

            const data = await response.text();
            console.log('Command response:', data);
            return data;
        } catch (error) {
            console.error('Error sending command:', error);
            throw error;
        }
    }

    updateBoardAfterShot(x, y, response, isEnemyBoard) {
        const board = isEnemyBoard ? this.enemyBoard : this.playerBoard;
        const cell = board.querySelector(`.grid .cell[data-x="${x}"][data-y="${y}"]`);
        
        if (!cell) {
            console.error('Cell not found:', x, y);
            return;
        }
        
        cell.classList.remove('hit', 'miss', 'kill');
        
        if (response.includes('destroyed')) {
            cell.classList.add('hit');
            cell.classList.add('kill');
            this.markSurroundingCells(board, x, y);
        } else if (response.includes('Hit')) {
            cell.classList.add('hit');
        } else if (response.includes('Miss')) {
            cell.classList.add('miss');
        }
    }

    markSurroundingCells(board, x, y) {
        for (let dx = -1; dx <= 1; dx++) {
            for (let dy = -1; dy <= 1; dy++) {
                if (dx === 0 && dy === 0) continue;
                
                const newX = x + dx;
                const newY = y + dy;
                
                if (newX >= 0 && newX < this.width && newY >= 0 && newY < this.height) {
                    const surroundingCell = board.querySelector(`.grid .cell[data-x="${newX}"][data-y="${newY}"]`);
                    if (surroundingCell && !surroundingCell.classList.contains('hit') && !surroundingCell.classList.contains('miss')) {
                        surroundingCell.classList.add('miss');
                    }
                }
            }
        }
    }

    async updateBoards() {
        console.log('Updating boards...');
        return fetch('/state')
            .then(response => {
                if (!response.ok) {
                    throw new Error(`HTTP error! status: ${response.status}`);
                }
                return response.text();
            })
            .then(text => {
                console.log('Raw state response:', text);
                
                document.querySelectorAll('.cell').forEach(cell => {
                    cell.classList.remove('ship', 'hit', 'miss', 'kill', 'surrounding');
                });

                const lines = text.split('\n');
                let currentBoard = null;
                let rowIndex = 0;
                
                for (const line of lines) {
                    const trimmedLine = line.trim();
                    
                    if (trimmedLine.includes('Your ships:')) {
                        currentBoard = 'player';
                        rowIndex = 0;
                        continue;
                    } else if (trimmedLine.includes('Enemy ships:')) {
                        currentBoard = 'enemy';
                        rowIndex = 0;
                        continue;
                    }

                    if (!trimmedLine || trimmedLine.includes('ships:')) {
                        continue;
                    }

                    if (currentBoard) {
                        const row = trimmedLine.split('');
                        row.forEach((cell, x) => {
                            const board = currentBoard === 'player' ? this.playerBoard : this.enemyBoard;
                            const cellElement = board.querySelector(`.grid .cell[data-x="${x}"][data-y="${rowIndex}"]`);
                            if (cellElement) {
                                if (cell === 'S' && currentBoard === 'player') {
                                    cellElement.classList.add('ship');
                                } else if (cell === 'X') {
                                    cellElement.classList.add('hit');
                                } else if (cell === 'O') {
                                    cellElement.classList.add('miss');
                                }
                            }
                        });
                        rowIndex++;
                    }
                }
            })
            .catch(error => {
                console.error('Error updating boards:', error);
            });
    }

    updateCell(board, x, y, state) {
        const cell = board.querySelector(`.grid .cell[data-x="${x}"][data-y="${y}"]`);
        if (cell) {
            cell.classList.add(state);
        }
    }

    appendToConsole(text) {
        if (!text) return;
        
        const newLine = document.createTextNode(text + '\n');
        this.terminalOutput.appendChild(newLine);
        this.terminalOutput.scrollTop = this.terminalOutput.scrollHeight;
    }

    showGameOver(isWin) {
        const gameOverDiv = document.getElementById('game-over');
        if (!gameOverDiv) return;
        
        gameOverDiv.textContent = isWin ? 'You Won!' : 'You Lost!';
        gameOverDiv.className = isWin ? 'game-over win' : 'game-over lose';
        gameOverDiv.style.display = 'flex';
        
        setTimeout(() => {
            gameOverDiv.style.display = 'none';
        }, 3000);
    }
}

document.addEventListener('DOMContentLoaded', () => {
    window.game = new Game();
});
