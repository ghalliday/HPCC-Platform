// Sample test data structure
const testInfo = {
    regression: {
        title: 'Regression Tests',
        description: 'A comprehensive suite of self-contained tests designed to verify the basic functionality of the HPCC Platform. These tests ensure that existing features continue to work correctly after code changes.',
        criteria: 'Pass/Fail based on test execution. Any test failure indicates a regression that needs immediate attention.',
        details: [
            'Tests core ECL language features',
            'Validates data processing operations',
            'Checks system integration points',
            'Approximately 1,250 individual test cases'
        ]
    },
    obt: {
        title: 'OBT (Overnight Build Tests)',
        description: 'More extensive test suite that runs during overnight builds. These tests cover advanced scenarios and edge cases that require longer execution times.',
        criteria: 'Pass/Fail with performance monitoring. Tests are expected to complete within defined time windows.',
        details: [
            'Extended integration testing',
            'Complex data transformations',
            'Multi-node cluster operations',
            'Approximately 900 test scenarios'
        ]
    },
    bvt: {
        title: 'BVT (Build Verification Tests)',
        description: 'Integration tests that validate the platform works correctly as a complete system. These tests verify critical user workflows and system interactions.',
        criteria: 'All tests must pass for a build to be considered stable for release.',
        details: [
            'End-to-end workflow validation',
            'Cross-component integration',
            'System startup and configuration',
            'Approximately 160 critical scenarios'
        ]
    },
    coverage: {
        title: 'Code Coverage',
        description: 'Measures the proportion of code (functions, lines, and execution paths) that are executed during test runs. Higher coverage indicates more thorough testing.',
        criteria: 'Target: >80% line coverage. Trend should be stable or improving.',
        details: [
            'Line coverage metrics',
            'Function coverage analysis',
            'Branch/path coverage tracking',
            'Coverage trends over time'
        ]
    },
    performance: {
        title: 'Performance Tests',
        description: 'Specific tests designed to measure the performance of key platform operations. Results are compared against established baselines to detect performance regressions.',
        criteria: 'Deviation from baseline should be within ±5%. Larger deviations require investigation.',
        details: [
            'Query execution timing',
            'Data throughput measurements',
            'Resource utilization tracking',
            'Comparison against historical baselines'
        ]
    }
};

// Initialize dashboard
document.addEventListener('DOMContentLoaded', function() {
    initializeEventListeners();
    loadTestData();
});

function initializeEventListeners() {
    // Date selector
    document.getElementById('test-date').addEventListener('change', handleDateChange);
    document.getElementById('refresh-btn').addEventListener('click', refreshData);
    
    // View controls
    document.getElementById('compare-mode-btn').addEventListener('click', enableCompareMode);
    document.getElementById('export-btn').addEventListener('click', exportData);
    
    // Test cells
    document.querySelectorAll('.test-cell').forEach(cell => {
        cell.addEventListener('click', handleTestCellClick);
    });
    
    // Info buttons
    document.querySelectorAll('.info-btn').forEach(btn => {
        btn.addEventListener('click', handleInfoClick);
    });
    
    // Modal close
    const modal = document.getElementById('test-info-modal');
    const closeBtn = document.querySelector('.close');
    closeBtn.addEventListener('click', () => {
        modal.style.display = 'none';
    });
    
    window.addEventListener('click', (event) => {
        if (event.target === modal) {
            modal.style.display = 'none';
        }
    });
}

function handleDateChange(event) {
    const selectedDate = event.target.value;
    console.log('Loading data for date:', selectedDate);
    // TODO: Fetch data for selected date
    showNotification(`Loading data for ${selectedDate}...`);
}

function refreshData() {
    console.log('Refreshing data...');
    showNotification('Refreshing test data...');
    // TODO: Implement data refresh
    setTimeout(() => {
        showNotification('Data refreshed successfully', 'success');
    }, 1000);
}

function enableCompareMode() {
    const btn = document.getElementById('compare-mode-btn');
    const isActive = btn.classList.toggle('active');
    
    if (isActive) {
        btn.textContent = 'Exit Compare Mode';
        btn.classList.remove('btn-secondary');
        btn.classList.add('btn-primary');
        showNotification('Compare mode enabled. Select two dates to compare.');
        // TODO: Show second date picker
    } else {
        btn.textContent = 'Compare Mode';
        btn.classList.remove('btn-primary');
        btn.classList.add('btn-secondary');
        showNotification('Compare mode disabled');
    }
}

function exportData() {
    console.log('Exporting data...');
    showNotification('Preparing export...', 'info');
    // TODO: Implement export functionality (CSV, JSON, etc.)
    setTimeout(() => {
        showNotification('Export complete', 'success');
    }, 1000);
}

function handleTestCellClick(event) {
    const cell = event.currentTarget;
    const test = cell.dataset.test;
    const platform = cell.dataset.platform;
    const version = cell.dataset.version;
    
    console.log(`Clicked test cell: ${test}, ${platform}, ${version}`);
    
    // Navigate to detail page
    // TODO: Implement navigation to build detail page
    const url = `build-detail.html?version=${version}&platform=${platform}&test=${test}&date=${document.getElementById('test-date').value}`;
    console.log('Would navigate to:', url);
    
    showNotification(`Loading details for ${version} ${platform} ${test}...`);
}

function handleInfoClick(event) {
    event.stopPropagation();
    const test = event.target.dataset.test;
    const info = testInfo[test];
    
    if (info) {
        showTestInfoModal(info);
    }
}

function showTestInfoModal(info) {
    const modal = document.getElementById('test-info-modal');
    const title = document.getElementById('modal-title');
    const body = document.getElementById('modal-body');
    
    title.textContent = info.title;
    
    let html = `
        <p><strong>Description:</strong> ${info.description}</p>
        <p><strong>Success Criteria:</strong> ${info.criteria}</p>
        <p><strong>Key Details:</strong></p>
        <ul>
    `;
    
    info.details.forEach(detail => {
        html += `<li>${detail}</li>`;
    });
    
    html += `</ul>
        <p style="margin-top: 20px;">
            <a href="#" onclick="viewTestHistory('${info.title}'); return false;" 
               style="color: var(--color-primary); text-decoration: none; font-weight: 500;">
                View historical results →
            </a>
        </p>
    `;
    
    body.innerHTML = html;
    modal.style.display = 'block';
}

function viewTestHistory(testName) {
    console.log('Viewing history for:', testName);
    // TODO: Navigate to test history page
    showNotification(`Loading history for ${testName}...`);
    document.getElementById('test-info-modal').style.display = 'none';
}

function loadTestData() {
    // TODO: Load actual test data from API or data source
    console.log('Loading test data...');
    
    // Simulate data loading
    setTimeout(() => {
        updateSummaryCards();
        highlightRecentChanges();
    }, 500);
}

function updateSummaryCards() {
    // Calculate summary statistics from the grid
    const allCells = document.querySelectorAll('.test-cell');
    let passCount = 0;
    let warnCount = 0;
    let failCount = 0;
    
    allCells.forEach(cell => {
        if (cell.classList.contains('status-pass')) passCount++;
        else if (cell.classList.contains('status-warn')) warnCount++;
        else if (cell.classList.contains('status-fail')) failCount++;
    });
    
    const totalCount = allCells.length;
    const passRate = ((passCount / totalCount) * 100).toFixed(1);
    
    console.log(`Summary: ${passRate}% pass rate, ${failCount} failures, ${warnCount} warnings`);
}

function highlightRecentChanges() {
    // TODO: Highlight cells that have changed status recently
    console.log('Highlighting recent changes...');
}

function showNotification(message, type = 'info') {
    // Simple notification system
    const notification = document.createElement('div');
    notification.textContent = message;
    notification.style.cssText = `
        position: fixed;
        top: 20px;
        right: 20px;
        background: ${type === 'success' ? '#28a745' : type === 'error' ? '#dc3545' : '#0066cc'};
        color: white;
        padding: 12px 24px;
        border-radius: 6px;
        box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
        z-index: 10000;
        animation: slideInRight 0.3s ease;
    `;
    
    document.body.appendChild(notification);
    
    setTimeout(() => {
        notification.style.animation = 'slideOutRight 0.3s ease';
        setTimeout(() => {
            document.body.removeChild(notification);
        }, 300);
    }, 3000);
}

// Add CSS animations for notifications
const style = document.createElement('style');
style.textContent = `
    @keyframes slideInRight {
        from {
            transform: translateX(400px);
            opacity: 0;
        }
        to {
            transform: translateX(0);
            opacity: 1;
        }
    }
    
    @keyframes slideOutRight {
        from {
            transform: translateX(0);
            opacity: 1;
        }
        to {
            transform: translateX(400px);
            opacity: 0;
        }
    }
`;
document.head.appendChild(style);
