import Box from '@mui/material/Box'
import Button from '@mui/material/Button'
import Card from '@mui/material/Card'
import CardContent from '@mui/material/CardContent'
import Stack from '@mui/material/Stack'
import Typography from '@mui/material/Typography'

export default function Settings({ rows, health, onClearAll }) {
  return (
    <Box sx={{ display: 'flex', flexDirection: 'column', gap: 3 }}>
      <Box>
        <Typography variant="h4" gutterBottom>
          Settings
        </Typography>
        <Typography color="text.secondary">
          Local configuration for this standalone dashboard.
        </Typography>
      </Box>

      <Stack direction={{ xs: 'column', md: 'row' }} spacing={2}>
        <Card sx={{ flex: 1 }}>
          <CardContent>
            <Typography variant="h6" gutterBottom>
              Stored Sites
            </Typography>
            <Typography variant="body1">{rows.length} blocked site(s)</Typography>
            <Typography color="text.secondary" sx={{ mt: 1 }}>
              The list is stored by the local CS Guardian API service.
            </Typography>
          </CardContent>
        </Card>

        <Card sx={{ flex: 1 }}>
          <CardContent>
            <Typography variant="h6" gutterBottom>
              Service Status
            </Typography>
            <Typography variant="body1">
              {health?.status === 'ok' ? 'Online' : 'Unavailable'}
            </Typography>
            <Typography color="text.secondary" sx={{ mt: 1 }}>
              Enforcement mode: {health?.enforcementMode || 'unknown'}
            </Typography>
            <Typography color="text.secondary">
              Hosts file: {health?.hostsFile || 'unknown'}
            </Typography>
            <Typography color={health?.enforcement?.ok ? 'success.main' : 'error.main'} sx={{ mt: 1 }}>
              {health?.enforcement?.message || 'No enforcement status available.'}
            </Typography>
            <Typography color="text.secondary">
              DNS flushed: {health?.enforcement?.dnsFlushed ? 'yes' : 'no'}
            </Typography>
            <Typography color="text.secondary">
              Last sync: {health?.enforcement?.syncedAt || 'never'}
            </Typography>
            <Typography color="text.secondary">
              Data file: {health?.dataFile || 'unknown'}
            </Typography>
          </CardContent>
        </Card>
      </Stack>

      <Card>
        <CardContent>
          <Typography variant="h6" gutterBottom>
            Reset Data
          </Typography>
          <Typography color="text.secondary" sx={{ mb: 2 }}>
            Clear the stored block list on the backend service and remove its managed entries from the hosts file.
          </Typography>
          <Button color="error" variant="outlined" onClick={onClearAll}>
            Clear All Sites
          </Button>
        </CardContent>
      </Card>
    </Box>
  )
}
