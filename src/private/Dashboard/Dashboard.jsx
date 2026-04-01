import { useEffect, useState } from 'react'
import AppBar from '@mui/material/AppBar'
import Box from '@mui/material/Box'
import Drawer from '@mui/material/Drawer'
import IconButton from '@mui/material/IconButton'
import List from '@mui/material/List'
import ListItemButton from '@mui/material/ListItemButton'
import ListItemIcon from '@mui/material/ListItemIcon'
import ListItemText from '@mui/material/ListItemText'
import Toolbar from '@mui/material/Toolbar'
import Typography from '@mui/material/Typography'
import ArrowBackIosNewIcon from '@mui/icons-material/ArrowBackIosNew'
import MenuIcon from '@mui/icons-material/Menu'
import RouterIcon from '@mui/icons-material/Router'
import SettingsIcon from '@mui/icons-material/Settings'
import Alert from '@mui/material/Alert'
import CircularProgress from '@mui/material/CircularProgress'
import AccessControl from './SubPages/AccessControl/AccessControl'
import Settings from './SubPages/Settings'
import {
  addSite as addSiteRequest,
  clearSites as clearSitesRequest,
  fetchServiceHealth,
  fetchSites,
  removeSite as removeSiteRequest,
} from '../../api/blocklistApi'

const DRAWER_WIDTH = 240

export default function Dashboard() {
  const [open, setOpen] = useState(false)
  const [selected, setSelected] = useState('AccessControl')
  const [sites, setSites] = useState([])
  const [health, setHealth] = useState(null)
  const [isLoading, setIsLoading] = useState(true)
  const [loadError, setLoadError] = useState('')

  useEffect(() => {
    let active = true

    async function loadDashboard() {
      setIsLoading(true)
      setLoadError('')

      try {
        const [sitesPayload, healthPayload] = await Promise.all([
          fetchSites(),
          fetchServiceHealth(),
        ])

        if (!active) {
          return
        }

        setSites(sitesPayload.sites)
        setHealth(healthPayload)
      } catch (error) {
        if (!active) {
          return
        }

        setLoadError(error.message)
      } finally {
        if (active) {
          setIsLoading(false)
        }
      }
    }

    loadDashboard()

    return () => {
      active = false
    }
  }, [])

  const refreshHealth = async () => {
    const nextHealth = await fetchServiceHealth()
    setHealth(nextHealth)
  }

  const addSite = async (site) => {
    const payload = await addSiteRequest(site)
    setSites(payload.sites)
    await refreshHealth()
  }

  const removeSite = async (site) => {
    const payload = await removeSiteRequest(site)
    setSites(payload.sites)
    await refreshHealth()
  }

  const clearSites = async () => {
    const payload = await clearSitesRequest()
    setSites(payload.sites)
    await refreshHealth()
  }

  const page = selected === 'Settings'
    ? (
        <Settings
          rows={sites}
          health={health}
          onClearAll={clearSites}
        />
      )
    : (
        <AccessControl
          rows={sites}
          onAddSite={addSite}
          onRemoveSite={removeSite}
        />
      )

  return (
    <Box sx={{ display: 'flex', minHeight: '100svh', bgcolor: '#f4f6fb' }}>
      <AppBar position="fixed">
        <Toolbar>
          <IconButton color="inherit" onClick={() => setOpen((value) => !value)}>
            <MenuIcon />
          </IconButton>
          <Box
            sx={{
              flexGrow: 1,
              ml: open ? `${DRAWER_WIDTH}px` : 0,
              transition: 'margin 0.3s ease',
            }}
          >
            <Typography variant="h6">CS Guardian</Typography>
          </Box>
        </Toolbar>
      </AppBar>

      <Drawer variant="persistent" anchor="left" open={open}>
        <Box sx={{ width: DRAWER_WIDTH }}>
          <IconButton
            color="inherit"
            onClick={() => setOpen(false)}
            sx={{ m: 1 }}
          >
            <ArrowBackIosNewIcon />
          </IconButton>
          <Toolbar />
          <List>
            <ListItemButton
              selected={selected === 'AccessControl'}
              onClick={() => setSelected('AccessControl')}
            >
              <ListItemIcon>
                <RouterIcon />
              </ListItemIcon>
              <ListItemText primary="Access Control" />
            </ListItemButton>
            <ListItemButton
              selected={selected === 'Settings'}
              onClick={() => setSelected('Settings')}
            >
              <ListItemIcon>
                <SettingsIcon />
              </ListItemIcon>
              <ListItemText primary="Settings" />
            </ListItemButton>
          </List>
        </Box>
      </Drawer>

      <Box
        component="main"
        sx={{
          flexGrow: 1,
          p: 3,
          ml: open ? `${DRAWER_WIDTH}px` : 0,
          transition: 'margin 0.3s ease',
        }}
      >
        <Toolbar />
        {isLoading ? (
          <Box sx={{ display: 'flex', justifyContent: 'center', pt: 8 }}>
            <CircularProgress />
          </Box>
        ) : loadError ? (
          <Alert severity="error">
            Failed to connect to the CS Guardian API. {loadError}
          </Alert>
        ) : (
          page
        )}
      </Box>
    </Box>
  )
}
