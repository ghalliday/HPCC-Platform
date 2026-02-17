# HPCC Testing Dashboard

A comprehensive web-based dashboard for monitoring HPCC Platform testing results across multiple versions and platforms.

## Overview

This dashboard provides a visual interface for tracking test results across:
- **5 Versions**: 9.12.x, 9.14.x, 10.0.x, 10.2.x, and master
- **3 Platforms**: hthor, Thor, and Roxie
- **5 Test Types**: Regression, OBT, BVT, Coverage, and Performance

## Features

### Main Dashboard (`index.html`)
- **High-level summary cards** showing overall pass rates and critical metrics
- **Interactive test grid** with color-coded status indicators:
  - 🟢 Green: Passing (tests passed, performance within acceptable range)
  - 🟡 Yellow: Warning (minor issues, performance slightly degraded)
  - 🔴 Red: Failing (test failures, significant performance issues)
- **Filtering capabilities** to show all, passing, warning, or failing tests
- **Date selection** for viewing historical results
- **Test information modals** explaining each test type
- **Clickable cells** for drilling down into detailed results

### Design Highlights
- Clean, modern interface with professional color scheme
- Responsive design that works on desktop and tablet
- Smooth animations and hover effects for better UX
- Accessible with clear visual indicators
- Print-friendly styling

## File Structure

```
testing/dashboard/
├── index.html           # Main dashboard page
├── css/
│   └── dashboard.css   # Styling and layout
├── js/
│   └── dashboard.js    # Interactivity and data handling
└── README.md           # This file
```

## Usage

### Opening the Dashboard

Simply open `index.html` in a web browser:
```bash
# Windows
start testing\dashboard\index.html

# Or navigate to the file and double-click
```

### Interacting with the Dashboard

1. **View test results**: Each cell shows pass/fail counts and percentages
2. **Filter results**: Click filter buttons to show only passing/warning/failing tests
3. **Change date**: Use the date picker to view historical results
4. **Get test info**: Click ⓘ buttons in column headers to learn about each test type
5. **Drill down**: Click any test cell to view detailed results (to be implemented)
6. **Compare dates**: Enable compare mode to see differences between two test runs

## Current Status

### Implemented
- ✅ Main dashboard layout and structure
- ✅ Responsive grid showing all versions and platforms
- ✅ Color-coded status indicators
- ✅ Summary statistics cards
- ✅ Filter functionality
- ✅ Test information modals
- ✅ Interactive hover effects
- ✅ Date selection UI

### To Be Implemented
- ⏳ Data loading from actual test results (currently uses mock data)
- ⏳ Build detail page
- ⏳ Test history page
- ⏳ Compare mode functionality
- ⏳ Data export (CSV/JSON)
- ⏳ Real-time refresh
- ⏳ Navigation between pages

## Data Integration

The dashboard currently displays mock data. To integrate with real test results:

1. **Define data source**: Determine where test results are stored (files, database, API)
2. **Update `loadTestData()` function**: Fetch real data instead of using hardcoded values
3. **Implement data refresh**: Add periodic polling or webhook integration
4. **Add error handling**: Handle network failures and missing data gracefully

### Expected Data Format

```javascript
{
  "date": "2026-02-17",
  "versions": {
    "9.12.x": {
      "hthor": {
        "regression": {
          "passed": 1245,
          "total": 1250,
          "status": "pass"
        },
        // ... other tests
      },
      // ... other platforms
    },
    // ... other versions
  }
}
```

## Customization

### Colors
Update CSS variables in `:root` selector in `dashboard.css`:
```css
:root {
    --color-primary: #0066cc;
    --color-success: #28a745;
    --color-warning: #ffc107;
    --color-danger: #dc3545;
    /* ... other variables */
}
```

### Test Types
Add or modify test definitions in `testInfo` object in `dashboard.js`:
```javascript
const testInfo = {
    newtest: {
        title: 'New Test Type',
        description: '...',
        criteria: '...',
        details: [...]
    }
};
```

## Browser Support

- Chrome/Edge (latest)
- Firefox (latest)
- Safari (latest)

## Next Steps

1. Create `build-detail.html` for detailed build results
2. Create `test-detail.html` for test-specific analysis
3. Implement data loading from actual test results
4. Add navigation between pages
5. Implement comparison mode
6. Add export functionality
7. Consider adding charts/graphs for trends

## License

Part of the HPCC Platform project.
